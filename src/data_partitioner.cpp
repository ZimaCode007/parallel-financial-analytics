#include "data_partitioner.h"

#include <algorithm>

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
