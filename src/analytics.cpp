#include "analytics.h"

#include <algorithm>
#include <climits>
#include <numeric>

namespace Analytics {

/* ── Basic aggregates ──────────────────────────────────────────────── */

/**
 * Compute the total amount_cents for all transactions in [begin, end).
 *
 * @param begin  Pointer to the first Transaction in the range.
 * @param end    Pointer one past the last Transaction (exclusive).
 * @return       Sum of amount_cents in cents; 0 for an empty range.
 */
long long sum(const Transaction* begin, const Transaction* end) {
    long long total = 0;
    for (auto it = begin; it != end; ++it) {
        total += it->amount_cents;
    }
    return total;
}

/**
 * Compute the average amount_cents for transactions in [begin, end).
 *
 * @param begin  Pointer to the first Transaction in the range.
 * @param end    Pointer one past the last Transaction (exclusive).
 * @return       Average amount in cents; 0.0 for an empty range.
 */
double avg(const Transaction* begin, const Transaction* end) {
    if (begin == end) return 0.0;
    return static_cast<double>(sum(begin, end)) /
           static_cast<double>(end - begin);
}

/**
 * Find the maximum amount_cents in [begin, end).
 *
 * @param begin  Pointer to the first Transaction in the range.
 * @param end    Pointer one past the last Transaction (exclusive).
 * @return       Maximum amount in cents; LLONG_MIN for an empty range.
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
 * Return the number of transactions in [begin, end).
 *
 * @param begin  Pointer to the first Transaction in the range.
 * @param end    Pointer one past the last Transaction (exclusive).
 * @return       Length of the range (number of transactions).
 */
size_t count(const Transaction* begin, const Transaction* end) {
    return static_cast<size_t>(end - begin);
}

/* ── Group-by aggregates ─────────────────────────────────────────── */

/**
 * Aggregate total amount_cents grouped by merchant_category.
 *
 * @param begin  Pointer to the first Transaction in the range.
 * @param end    Pointer one past the last Transaction (exclusive).
 * @return       Map of category string → total cents.
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
 * Aggregate total amount_cents grouped by US state code.
 *
 * @param begin  Pointer to the first Transaction in the range.
 * @param end    Pointer one past the last Transaction (exclusive).
 * @return       Map of state code (e.g. "CA") → total cents.
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
 * Merge src into dst by summing values for matching keys.
 *
 * Used to combine partial group-by maps produced by individual threads or
 * MPI ranks during parallel execution.
 *
 * @param dst  Destination map; updated in place.
 * @param src  Source map (read-only); its values are added to dst.
 */
void merge_group(std::unordered_map<std::string, long long>& dst,
                 const std::unordered_map<std::string, long long>& src) {
    for (const auto& [key, val] : src) {
        dst[key] += val;
    }
}

/* ── Sequential full-analytics baseline ─────────────────────────── */

/**
 * Run all analytics sequentially on the full dataset; used as the performance baseline.
 *
 * Each aggregate function performs its own pass over the data since a single
 * combined pass would complicate the code without measurable benefit at this scale.
 *
 * @param records  Full transaction vector (read-only).
 * @return         AnalyticsResult containing count, sum, avg, max, and both group-by maps.
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
