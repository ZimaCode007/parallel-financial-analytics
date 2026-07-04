#include "mpi_engine.h"

#include <mpi.h>
#include <cstring>
#include <stdexcept>
#include "data_partitioner.h"

/* ── Constructor ─────────────────────────────────────────────────── */

/**
 * Constructor: read this process's rank and communicator size from the MPI runtime.
 *
 * Must be constructed after MPI_Init; behaviour is undefined otherwise.
 * rank_ and world_size_ are immutable for the lifetime of the object.
 */
MpiEngine::MpiEngine() {
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size_);
}

/* ── Main entry point ────────────────────────────────────────────── */

/**
 * Run the full analytics suite in distributed parallel using MPI.
 *
 * All ranks must call this collectively.  Execution steps:
 *   1. Rank 0 partitions the dataset and broadcasts per-rank chunk sizes.
 *   2. Rank 0 serialises transactions into PackedTx and scatters them via MPI_Scatterv.
 *   3. Each rank independently computes local sum, max, and two group-by maps.
 *   4. MPI_Reduce collects scalar results on rank 0.
 *   5. Each rank serialises its group-by maps; MPI_Gatherv collects them on rank 0
 *      which then deserialises and merges them into the final result.
 *
 * @param records  Full dataset (only rank 0 needs valid data; other ranks pass empty).
 * @return         AnalyticsResult valid only on rank 0; other ranks return a default value.
 */
Analytics::AnalyticsResult MpiEngine::run(const std::vector<Transaction>& records) const {
    /* ── Step 1: Root partitions and scatters chunks ──────────────── */

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

    // Fixed-size record for MPI transfer.  All fields are POD so the struct
    // has no hidden padding on LP64 platforms (verified by static_assert).
    struct PackedTx {
        long long id;
        long long amount_cents;
        char      category[64];
        char      state[8];
    };
    static_assert(sizeof(PackedTx) == 88, "PackedTx must be exactly 88 bytes");

    // Register a contiguous MPI datatype sized to one PackedTx so that
    // Scatterv counts are in transactions (not bytes), avoiding unit confusion.
    MPI_Datatype mpi_packed_tx;
    MPI_Type_contiguous(static_cast<int>(sizeof(PackedTx)), MPI_BYTE, &mpi_packed_tx);
    MPI_Type_commit(&mpi_packed_tx);

    std::vector<PackedTx> send_buf;
    std::vector<int> displs(world_size_, 0);

    if (rank_ == 0) {
        send_buf.resize(records.size());
        for (size_t i = 0; i < records.size(); ++i) {
            send_buf[i].id           = records[i].id;
            send_buf[i].amount_cents = records[i].amount_cents;

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

    // counts and displs are now in units of mpi_packed_tx (one transaction each).
    MPI_Scatterv(
        send_buf.data(), chunk_sizes.data(), displs.data(), mpi_packed_tx,
        recv_buf.data(), my_chunk_size, mpi_packed_tx,
        0, MPI_COMM_WORLD);

    MPI_Type_free(&mpi_packed_tx);

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

    // Gather buffer sizes first so root can allocate the receive buffer.
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

/**
 * Serialise a group-by map to a byte buffer for MPI transfer.
 *
 * Wire format per entry: [ key_len (4 bytes) | key bytes | value (8 bytes) ]
 * Both key_len and value use native byte order (safe within a homogeneous MPI job).
 *
 * @param m   Map of string → long long to serialise.
 * @return    Byte vector suitable for passing directly to MPI_Send / MPI_Gatherv.
 */
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

/**
 * Deserialise a byte buffer produced by serialise_map back into a group-by map.
 *
 * Reads [ key_len | key | value ] records in sequence; stops early on truncation
 * or malformed data (remaining bytes are silently ignored).
 * Values for duplicate keys are summed, so multiple serialised partial maps can be
 * concatenated and deserialised in a single pass.
 *
 * @param buf  Byte buffer produced by serialise_map.
 * @return     Deserialised map of string → long long.
 */
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
