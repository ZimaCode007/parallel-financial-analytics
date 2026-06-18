#include "analytics.h"

#include <algorithm>
#include <climits>
#include <numeric>

namespace Analytics {

/* ── Basic aggregates ──────────────────────────────────────────────── */

long long sum(const Transaction* begin, const Transaction* end) {
    long long total = 0;
    for (auto it = begin; it != end; ++it) {
        total += it->amount_cents;
    }
    return total;
}

double avg(const Transaction* begin, const Transaction* end) {
    if (begin == end) return 0.0;
    return static_cast<double>(sum(begin, end)) /
           static_cast<double>(end - begin);
}

long long max_amount(const Transaction* begin, const Transaction* end) {
    if (begin == end) return LLONG_MIN;
    long long mx = begin->amount_cents;
    for (auto it = begin + 1; it != end; ++it) {
        if (it->amount_cents > mx) mx = it->amount_cents;
    }
    return mx;
}

size_t count(const Transaction* begin, const Transaction* end) {
    return static_cast<size_t>(end - begin);
}

/* ── Group-by aggregates ─────────────────────────────────────────── */

std::unordered_map<std::string, long long>
group_by_category(const Transaction* begin, const Transaction* end) {
    std::unordered_map<std::string, long long> result;
    for (auto it = begin; it != end; ++it) {
        result[it->merchant_category] += it->amount_cents;
    }
    return result;
}

std::unordered_map<std::string, long long>
group_by_state(const Transaction* begin, const Transaction* end) {
    std::unordered_map<std::string, long long> result;
    for (auto it = begin; it != end; ++it) {
        result[it->state] += it->amount_cents;
    }
    return result;
}

/* ── Merge helper ────────────────────────────────────────────────── */

void merge_group(std::unordered_map<std::string, long long>& dst,
                 const std::unordered_map<std::string, long long>& src) {
    for (const auto& [key, val] : src) {
        dst[key] += val;
    }
}

/* ── Sequential full-analytics baseline ─────────────────────────── */

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
