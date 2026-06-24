#include "analytics.h"

#include <algorithm>
#include <climits>
#include <numeric>

namespace Analytics {

/* ── Basic aggregates ──────────────────────────────────────────────── */

/**
 * 计算区间 [begin, end) 内所有交易金额的总和。
 *
 * @param begin  指向区间首个 Transaction 的指针
 * @param end    指向区间末尾（不含）的指针
 * @return       所有 amount_cents 之和，单位为分（cents）；空区间返回 0
 */
long long sum(const Transaction* begin, const Transaction* end) {
    long long total = 0;
    for (auto it = begin; it != end; ++it) {
        total += it->amount_cents;
    }
    return total;
}

/**
 * 计算区间 [begin, end) 内所有交易金额的平均值。
 *
 * @param begin  指向区间首个 Transaction 的指针
 * @param end    指向区间末尾（不含）的指针
 * @return       平均金额，单位为分（cents）；空区间返回 0.0
 */
double avg(const Transaction* begin, const Transaction* end) {
    if (begin == end) return 0.0;
    return static_cast<double>(sum(begin, end)) /
           static_cast<double>(end - begin);
}

/**
 * 查找区间 [begin, end) 内的最大交易金额。
 *
 * @param begin  指向区间首个 Transaction 的指针
 * @param end    指向区间末尾（不含）的指针
 * @return       最大 amount_cents，单位为分；空区间返回 LLONG_MIN
 */
long long max_amount(const Transaction* begin, const Transaction* end) {
    if (begin == end) return LLONG_MIN;
    long long mx = begin->amount_cents;
    for (auto it = begin + 1; it != end; ++it) {
        if (it->amount_cents > mx) mx = it->amount_cents;
    }
    return mx;
}

/**
 * 返回区间 [begin, end) 内的交易条数。
 *
 * @param begin  指向区间首个 Transaction 的指针
 * @param end    指向区间末尾（不含）的指针
 * @return       区间长度（即交易笔数）
 */
size_t count(const Transaction* begin, const Transaction* end) {
    return static_cast<size_t>(end - begin);
}

/* ── Group-by aggregates ─────────────────────────────────────────── */

/**
 * 按商户类别（merchant_category）对交易金额求和。
 *
 * @param begin  指向区间首个 Transaction 的指针
 * @param end    指向区间末尾（不含）的指针
 * @return       unordered_map，键为类别字符串，值为该类别交易总额（分）
 */
std::unordered_map<std::string, long long>
group_by_category(const Transaction* begin, const Transaction* end) {
    std::unordered_map<std::string, long long> result;
    for (auto it = begin; it != end; ++it) {
        result[it->merchant_category] += it->amount_cents;
    }
    return result;
}

/**
 * 按美国州代码（state）对交易金额求和。
 *
 * @param begin  指向区间首个 Transaction 的指针
 * @param end    指向区间末尾（不含）的指针
 * @return       unordered_map，键为州代码（如 "CA"），值为该州交易总额（分）
 */
std::unordered_map<std::string, long long>
group_by_state(const Transaction* begin, const Transaction* end) {
    std::unordered_map<std::string, long long> result;
    for (auto it = begin; it != end; ++it) {
        result[it->state] += it->amount_cents;
    }
    return result;
}

/* ── Merge helper ────────────────────────────────────────────────── */

/**
 * 将 src 中的分组统计数据合并到 dst，相同键的值相加。
 *
 * 用于并行场景下合并各线程或各 MPI rank 产生的局部 group-by 结果。
 *
 * @param dst  目标 map，合并后在原地更新
 * @param src  源 map（只读），其键值对被累加到 dst 中对应键上
 */
void merge_group(std::unordered_map<std::string, long long>& dst,
                 const std::unordered_map<std::string, long long>& src) {
    for (const auto& [key, val] : src) {
        dst[key] += val;
    }
}

/* ── Sequential full-analytics baseline ─────────────────────────── */

/**
 * 对完整数据集顺序执行全部分析操作，作为性能基准。
 *
 * 一次遍历无法满足所有指标（sum 和 group-by 各需独立遍历），
 * 此函数按顺序依次调用各聚合函数，结果存入 AnalyticsResult。
 *
 * @param records  完整交易记录向量（只读）
 * @return         包含 count、sum、avg、max 及两个 group-by 结果的 AnalyticsResult
 */
AnalyticsResult run_sequential(const std::vector<Transaction>& records) {
    const Transaction* begin = records.data();
    const Transaction* end   = records.data() + records.size();

    AnalyticsResult res;
    res.record_count   = count(begin, end);
    res.total_cents    = sum(begin, end);
    res.average_cents  = avg(begin, end);
    res.max_cents      = max_amount(begin, end);
    res.by_category    = group_by_category(begin, end);
    res.by_state       = group_by_state(begin, end);
    return res;
}

} // namespace Analytics
