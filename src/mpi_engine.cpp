#include "mpi_engine.h"

#include <mpi.h>
#include <cstring>
#include <stdexcept>
#include <climits>
#include "data_partitioner.h"

/* ── Constructor ─────────────────────────────────────────────────── */

MpiEngine::MpiEngine() {
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size_);
}

/* ── Main entry point ────────────────────────────────────────────── */

Analytics::AnalyticsResult MpiEngine::run(const std::vector<Transaction>& records) const {
    /* ── Step 1: Root partitions and scatters chunks ──────────────── */

    // Serialise each Transaction to a flat byte buffer for MPI transfer.
    // We use a fixed-size struct approach: encode each Transaction as a
    // newline-terminated CSV string and send per-rank byte arrays.
    // For simplicity we broadcast sizes first, then send data.

    // Build per-rank chunk sizes (in number of transactions) on root.
    std::vector<int> chunk_sizes(world_size_, 0);
    if (rank_ == 0) {
        auto ranges = DataPartitioner::partition_ranges(
            records.size(), static_cast<size_t>(world_size_));
        for (int i = 0; i < world_size_; ++i) {
            chunk_sizes[i] = static_cast<int>(ranges[i].second - ranges[i].first);
        }
    }

    // Distribute chunk sizes to all ranks.
    MPI_Bcast(chunk_sizes.data(), world_size_, MPI_INT, 0, MPI_COMM_WORLD);

    int my_chunk_size = chunk_sizes[rank_];

    /* ── Serialise and scatter transaction data ───────────────────── */

    // Each transaction is sent as a packed fixed-size record:
    //   [ id(8) | amount_cents(8) | category_len(4) | category(64) |
    //     state_len(4) | state(8) ]
    // Using a simple flat struct avoids a custom MPI datatype while
    // keeping the scatter straightforward.

    struct PackedTx {
        long long id;
        long long amount_cents;
        char      category[64];
        char      state[8];
    };
    static_assert(sizeof(PackedTx) == 8 + 8 + 64 + 8, "PackedTx layout mismatch");

    std::vector<PackedTx> send_buf;
    std::vector<int> displs(world_size_, 0);

    if (rank_ == 0) {
        send_buf.resize(records.size());
        for (size_t i = 0; i < records.size(); ++i) {
            send_buf[i].id           = records[i].id;
            send_buf[i].amount_cents = records[i].amount_cents;

            // strncpy with zero-fill ensures no uninitialised padding bytes.
            std::memset(send_buf[i].category, 0, sizeof(send_buf[i].category));
            std::strncpy(send_buf[i].category,
                         records[i].merchant_category.c_str(),
                         sizeof(send_buf[i].category) - 1);

            std::memset(send_buf[i].state, 0, sizeof(send_buf[i].state));
            std::strncpy(send_buf[i].state,
                         records[i].state.c_str(),
                         sizeof(send_buf[i].state) - 1);
        }
        for (int i = 1; i < world_size_; ++i) {
            displs[i] = displs[i - 1] + chunk_sizes[i - 1];
        }
    }

    std::vector<PackedTx> recv_buf(my_chunk_size);

    MPI_Scatterv(
        send_buf.data(), chunk_sizes.data(), displs.data(), MPI_BYTE,
        recv_buf.data(), my_chunk_size * static_cast<int>(sizeof(PackedTx)), MPI_BYTE,
        0, MPI_COMM_WORLD);

    /* ── Step 2: Each rank computes local aggregates ──────────────── */

    long long local_sum = 0;
    long long local_max = LLONG_MIN;

    std::unordered_map<std::string, long long> local_by_cat;
    std::unordered_map<std::string, long long> local_by_state;

    for (const auto& tx : recv_buf) {
        local_sum += tx.amount_cents;
        if (tx.amount_cents > local_max) local_max = tx.amount_cents;

        local_by_cat[tx.category]   += tx.amount_cents;
        local_by_state[tx.state]    += tx.amount_cents;
    }

    /* ── Step 3: Reduce scalar values to root ────────────────────── */

    long long global_sum = 0;
    long long global_max = 0;
    long long total_count_ll = static_cast<long long>(records.size());

    MPI_Reduce(&local_sum, &global_sum, 1, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_max, &global_max, 1, MPI_LONG_LONG_INT, MPI_MAX, 0, MPI_COMM_WORLD);

    /* ── Step 4: Gather group-by maps to root via serialisation ─── */

    auto cat_buf   = serialise_map(local_by_cat);
    auto state_buf = serialise_map(local_by_state);

    int cat_size   = static_cast<int>(cat_buf.size());
    int state_size = static_cast<int>(state_buf.size());

    // Gather buffer sizes first.
    std::vector<int> cat_sizes(world_size_), state_sizes(world_size_);
    MPI_Gather(&cat_size,   1, MPI_INT, cat_sizes.data(),   1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&state_size, 1, MPI_INT, state_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    Analytics::AnalyticsResult result;

    if (rank_ == 0) {
        // Compute displacements for Gatherv.
        auto make_displs = [&](const std::vector<int>& sizes) {
            std::vector<int> d(world_size_, 0);
            for (int i = 1; i < world_size_; ++i) d[i] = d[i-1] + sizes[i-1];
            return d;
        };
        auto cat_d   = make_displs(cat_sizes);
        auto state_d = make_displs(state_sizes);

        int total_cat   = cat_d.back()   + cat_sizes.back();
        int total_state = state_d.back() + state_sizes.back();

        std::vector<char> all_cat(total_cat), all_state(total_state);

        MPI_Gatherv(cat_buf.data(),   cat_size,   MPI_BYTE,
                    all_cat.data(),   cat_sizes.data(),   cat_d.data(),   MPI_BYTE,
                    0, MPI_COMM_WORLD);
        MPI_Gatherv(state_buf.data(), state_size, MPI_BYTE,
                    all_state.data(), state_sizes.data(), state_d.data(), MPI_BYTE,
                    0, MPI_COMM_WORLD);

        // Deserialise and merge all per-rank maps.
        for (int i = 0; i < world_size_; ++i) {
            std::vector<char> c_slice(all_cat.begin()   + cat_d[i],
                                      all_cat.begin()   + cat_d[i]   + cat_sizes[i]);
            std::vector<char> s_slice(all_state.begin() + state_d[i],
                                      all_state.begin() + state_d[i] + state_sizes[i]);
            Analytics::merge_group(result.by_category, deserialise_map(c_slice));
            Analytics::merge_group(result.by_state,    deserialise_map(s_slice));
        }

        result.record_count  = static_cast<size_t>(total_count_ll);
        result.total_cents   = global_sum;
        result.max_cents     = global_max;
        result.average_cents = (result.record_count > 0)
                               ? static_cast<double>(global_sum) /
                                 static_cast<double>(result.record_count)
                               : 0.0;
    } else {
        // Non-root ranks still participate in Gatherv.
        MPI_Gatherv(cat_buf.data(),   cat_size,   MPI_BYTE,
                    nullptr, nullptr, nullptr, MPI_BYTE, 0, MPI_COMM_WORLD);
        MPI_Gatherv(state_buf.data(), state_size, MPI_BYTE,
                    nullptr, nullptr, nullptr, MPI_BYTE, 0, MPI_COMM_WORLD);
    }

    return result;  // Only meaningful on rank 0.
}

/* ── Serialisation ───────────────────────────────────────────────── */

// Wire format: repeated [ key_len(4 bytes) | key bytes | value(8 bytes) ]
std::vector<char> MpiEngine::serialise_map(
    const std::unordered_map<std::string, long long>& m)
{
    std::vector<char> buf;
    for (const auto& [key, val] : m) {
        int klen = static_cast<int>(key.size());
        // Append key length.
        buf.insert(buf.end(),
                   reinterpret_cast<const char*>(&klen),
                   reinterpret_cast<const char*>(&klen) + sizeof(int));
        // Append key bytes.
        buf.insert(buf.end(), key.begin(), key.end());
        // Append value.
        buf.insert(buf.end(),
                   reinterpret_cast<const char*>(&val),
                   reinterpret_cast<const char*>(&val) + sizeof(long long));
    }
    return buf;
}

std::unordered_map<std::string, long long> MpiEngine::deserialise_map(
    const std::vector<char>& buf)
{
    std::unordered_map<std::string, long long> m;
    size_t pos = 0;
    while (pos < buf.size()) {
        if (pos + sizeof(int) > buf.size()) break;

        int klen = 0;
        std::memcpy(&klen, buf.data() + pos, sizeof(int));
        pos += sizeof(int);

        if (pos + static_cast<size_t>(klen) + sizeof(long long) > buf.size()) break;

        std::string key(buf.data() + pos, buf.data() + pos + klen);
        pos += klen;

        long long val = 0;
        std::memcpy(&val, buf.data() + pos, sizeof(long long));
        pos += sizeof(long long);

        m[key] += val;
    }
    return m;
}
