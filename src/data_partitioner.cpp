#include "data_partitioner.h"

#include <algorithm>

/**
 * 将交易记录数组按数量均分为 num_parts 个连续子数组（深拷贝）。
 *
 * 当总数不能整除时，靠前的分区各多分配一条记录，
 * 保证所有分区大小差距不超过 1。
 * 适用于 MPI：各 rank 需要独立的数据副本。
 *
 * @param records    完整交易记录数组（只读，不修改原数据）
 * @param num_parts  目标分区数；若大于 records.size() 则自动收窄
 * @return           num_parts 个 Transaction 向量构成的向量；
 *                   每个子向量对应一个数据分区
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
 * 计算将 total_size 个元素均分为 num_parts 份时各份的索引区间。
 *
 * 返回值中每个元素为 {start, end}，区间为左闭右开 [start, end)。
 * 不复制任何数据，适用于 OpenMP 等共享内存场景：
 * 线程直接用区间索引访问同一个原始数组。
 *
 * @param total_size  待分区的元素总数
 * @param num_parts   目标分区数；若大于 total_size 则自动收窄
 * @return            长度为 num_parts 的 {start, end} 对向量，
 *                    所有区间连续不重叠，合并后恰好覆盖 [0, total_size)
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
