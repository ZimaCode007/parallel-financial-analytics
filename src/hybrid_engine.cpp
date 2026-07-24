#include "hybrid_engine.h"

#include <climits>
#include <cstring>
#include <mpi.h>
#include <unordered_map>

#include "omp_engine.h"

namespace {

// Only fields used by the analytics functions are sent over MPI.
struct PackedTx {
    long long id;
    long long amount_cents;
    char category[64];
    char state[8];
};
static_assert(sizeof(PackedTx) == 88, "PackedTx layout changed");

std::vector<char> serialise_map(const std::unordered_map<std::string, long long>& map) {
    std::vector<char> buffer;
    for (const auto& [key, value] : map) {
        const int key_size = static_cast<int>(key.size());
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&key_size),
                      reinterpret_cast<const char*>(&key_size) + sizeof(key_size));
        buffer.insert(buffer.end(), key.begin(), key.end());
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&value),
                      reinterpret_cast<const char*>(&value) + sizeof(value));
    }
    return buffer;
}

std::unordered_map<std::string, long long> deserialise_map(const char* data, size_t size) {
    std::unordered_map<std::string, long long> map;
    size_t pos = 0;
    while (pos + sizeof(int) <= size) {
        int key_size = 0;
        std::memcpy(&key_size, data + pos, sizeof(key_size));
        pos += sizeof(key_size);
        if (key_size < 0 || static_cast<size_t>(key_size) > size - pos - sizeof(long long)) break;
        std::string key(data + pos, static_cast<size_t>(key_size));
        pos += static_cast<size_t>(key_size);
        long long value = 0;
        std::memcpy(&value, data + pos, sizeof(value));
        pos += sizeof(value);
        map[key] += value;
    }
    return map;
}

std::vector<int> displacements(const std::vector<int>& sizes) {
    std::vector<int> result(sizes.size(), 0);
    for (size_t i = 1; i < sizes.size(); ++i) result[i] = result[i - 1] + sizes[i - 1];
    return result;
}

} // namespace

HybridEngine::HybridEngine(int omp_threads) : omp_threads_(omp_threads) {
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size_);
    OmpEngine omp(omp_threads_);
    omp_threads_ = omp.num_threads();
}

Analytics::AnalyticsResult HybridEngine::run(const std::vector<Transaction>& records) const {
    std::vector<int> counts(world_size_, 0);
    std::vector<int> tx_displacements(world_size_, 0);
    if (is_root()) {
        const size_t base = records.size() / static_cast<size_t>(world_size_);
        const size_t remainder = records.size() % static_cast<size_t>(world_size_);
        for (int rank = 0; rank < world_size_; ++rank)
            counts[rank] = static_cast<int>(base + (static_cast<size_t>(rank) < remainder));
        tx_displacements = displacements(counts);
    }
    MPI_Bcast(counts.data(), world_size_, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<PackedTx> packed_records;
    if (is_root()) {
        packed_records.resize(records.size());
        for (size_t i = 0; i < records.size(); ++i) {
            auto& packed = packed_records[i];
            packed.id = records[i].id;
            packed.amount_cents = records[i].amount_cents;
            std::memset(packed.category, 0, sizeof(packed.category));
            std::memset(packed.state, 0, sizeof(packed.state));
            std::strncpy(packed.category, records[i].merchant_category.c_str(), sizeof(packed.category) - 1);
            std::strncpy(packed.state, records[i].state.c_str(), sizeof(packed.state) - 1);
        }
    }

    MPI_Datatype packed_type;
    MPI_Type_contiguous(static_cast<int>(sizeof(PackedTx)), MPI_BYTE, &packed_type);
    MPI_Type_commit(&packed_type);
    std::vector<PackedTx> local_packed(counts[rank_]);
    MPI_Scatterv(packed_records.data(), counts.data(), tx_displacements.data(), packed_type,
                 local_packed.data(), counts[rank_], packed_type, 0, MPI_COMM_WORLD);
    MPI_Type_free(&packed_type);

    std::vector<Transaction> local_records;
    local_records.reserve(local_packed.size());
    for (const auto& packed : local_packed) {
        Transaction tx{};
        tx.id = packed.id;
        tx.amount_cents = packed.amount_cents;
        tx.merchant_category = packed.category;
        tx.state = packed.state;
        local_records.push_back(std::move(tx));
    }

    // OpenMP performs the local half of the hybrid computation.
    OmpEngine omp(omp_threads_);
    const auto local = omp.run(local_records);
    const long long local_count = static_cast<long long>(local.record_count);
    const long long local_max = local.record_count == 0 ? LLONG_MIN : local.max_cents;
    long long global_sum = 0, global_count = 0, global_max = LLONG_MIN;
    MPI_Reduce(&local.total_cents, &global_sum, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_count, &global_count, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_max, &global_max, 1, MPI_LONG_LONG, MPI_MAX, 0, MPI_COMM_WORLD);

    const auto category_buffer = serialise_map(local.by_category);
    const auto state_buffer = serialise_map(local.by_state);
    const int category_size = static_cast<int>(category_buffer.size());
    const int state_size = static_cast<int>(state_buffer.size());
    std::vector<int> category_sizes(world_size_), state_sizes(world_size_);
    MPI_Gather(&category_size, 1, MPI_INT, category_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&state_size, 1, MPI_INT, state_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<int> category_displacements, state_displacements;
    std::vector<char> all_categories, all_states;
    if (is_root()) {
        category_displacements = displacements(category_sizes);
        state_displacements = displacements(state_sizes);
        all_categories.resize(category_displacements.back() + category_sizes.back());
        all_states.resize(state_displacements.back() + state_sizes.back());
    }
    MPI_Gatherv(category_buffer.data(), category_size, MPI_BYTE, all_categories.data(),
                category_sizes.data(), category_displacements.data(), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Gatherv(state_buffer.data(), state_size, MPI_BYTE, all_states.data(),
                state_sizes.data(), state_displacements.data(), MPI_BYTE, 0, MPI_COMM_WORLD);

    Analytics::AnalyticsResult result{};
    if (is_root()) {
        result.record_count = static_cast<size_t>(global_count);
        result.total_cents = global_sum;
        result.max_cents = global_count == 0 ? 0 : global_max;
        result.average_cents = global_count == 0 ? 0.0 : static_cast<double>(global_sum) / global_count;
        for (int rank = 0; rank < world_size_; ++rank) {
            Analytics::merge_group(result.by_category, deserialise_map(
                all_categories.data() + category_displacements[rank], category_sizes[rank]));
            Analytics::merge_group(result.by_state, deserialise_map(
                all_states.data() + state_displacements[rank], state_sizes[rank]));
        }
    }
    return result;
}
