#include "data_partitioner.h"

#include <algorithm>

/**
 * Divide the transaction array into num_parts contiguous sub-arrays (deep copy).
 *
 * When the total is not evenly divisible, earlier partitions each receive one
 * extra record so all partition sizes differ by at most 1.
 * Suitable for MPI where each rank needs an independent data copy.
 *
 * @param records    Full transaction array (read-only; original is not modified).
 * @param num_parts  Desired number of partitions; clamped to records.size() if larger.
 * @return           Vector of num_parts Transaction vectors, one per partition.
 */
std::vector<std::vector<Transaction>> DataPartitioner::partition(
    const std::vector<Transaction>& records,
    size_t num_parts)
{
    if (num_parts == 0) num_parts = 1;
    // Never create more partitions than records (would produce empty chunks).
    num_parts = std::min(num_parts, records.size());

    auto ranges = partition_ranges(records.size(), num_parts);

    std::vector<std::vector<Transaction>> chunks;
    chunks.reserve(ranges.size());

    for (auto [start, end] : ranges) {
        chunks.emplace_back(records.begin() + start, records.begin() + end);
    }

    return chunks;
}

/**
 * Compute the [start, end) index ranges for dividing total_size elements
 * into num_parts equal (±1) partitions.
 *
 * No data is copied, making this suitable for OpenMP shared-memory use:
 * threads access the same underlying array via their assigned index range.
 *
 * @param total_size  Total number of elements to partition.
 * @param num_parts   Desired number of partitions; clamped to total_size if larger.
 * @return            Vector of num_parts {start, end} pairs — contiguous,
 *                    non-overlapping, and together covering [0, total_size).
 */
std::vector<std::pair<size_t, size_t>> DataPartitioner::partition_ranges(
    size_t total_size,
    size_t num_parts)
{
    if (num_parts == 0) num_parts = 1;
    num_parts = std::min(num_parts, total_size);

    std::vector<std::pair<size_t, size_t>> ranges;
    ranges.reserve(num_parts);

    // Integer division: base chunk size and number of "fat" chunks that get +1.
    size_t base      = total_size / num_parts;
    size_t remainder = total_size % num_parts;

    size_t start = 0;
    for (size_t i = 0; i < num_parts; ++i) {
        // The first 'remainder' chunks each get one extra element.
        size_t size = base + (i < remainder ? 1 : 0);
        ranges.emplace_back(start, start + size);
        start += size;
    }

    return ranges;
}
