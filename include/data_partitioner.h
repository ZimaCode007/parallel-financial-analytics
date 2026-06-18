#pragma once

#include <vector>
#include "transaction.h"

/**
 * Data Partitioning Module
 *
 * Splits a flat vector of transactions into equal-sized (±1) chunks so that
 * each OpenMP thread or MPI rank receives a contiguous, non-overlapping slice
 * of the dataset.  Using contiguous slices avoids inter-thread cache-line
 * bouncing and makes MPI scatter straightforward.
 *
 * Usage (OpenMP – chunks for N threads):
 *   auto chunks = DataPartitioner::partition(records, omp_get_max_threads());
 *
 * Usage (MPI – one chunk per rank):
 *   auto chunks = DataPartitioner::partition(records, world_size);
 *   // then send chunks[rank] to each MPI rank
 */
class DataPartitioner {
public:
    /**
     * Divide @p records into @p num_parts contiguous slices.
     *
     * Returns a vector of vectors; the last partition may be slightly smaller
     * than the others when the dataset size is not evenly divisible.
     *
     * @param records   The full dataset (not modified).
     * @param num_parts Number of partitions; clamped to records.size() if larger.
     */
    static std::vector<std::vector<Transaction>> partition(
        const std::vector<Transaction>& records,
        size_t num_parts);

    /**
     * Lightweight alternative: return (start, end) index pairs instead of
     * copying data.  Useful for OpenMP where all threads share the same
     * memory and only need their index range.
     *
     * Each element is {start_index, end_index} (end is exclusive).
     */
    static std::vector<std::pair<size_t, size_t>> partition_ranges(
        size_t total_size,
        size_t num_parts);
};
