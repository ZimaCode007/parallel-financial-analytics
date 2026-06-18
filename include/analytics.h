#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "transaction.h"

/**
 * Financial Analytics Module
 *
 * Provides pure (stateless) aggregation functions that operate on an
 * arbitrary span of Transaction records.  Each function works on any
 * contiguous range [begin, end), making them composable with both the
 * sequential baseline and the parallel engines.
 *
 * All monetary results are returned in cents and converted to dollars
 * only at the presentation layer (main.cpp / performance reporter).
 *
 * Group-by results are returned as unordered_maps so that partial maps
 * produced by different threads/ranks can be merged with merge_group().
 */
namespace Analytics {

/* ── Basic aggregates ──────────────────────────────────────────────── */

/** Sum of all amount_cents in [begin, end). */
long long sum(const Transaction* begin, const Transaction* end);

/** Average amount_cents in [begin, end); returns 0 when range is empty. */
double    avg(const Transaction* begin, const Transaction* end);

/** Maximum amount_cents in [begin, end); returns LLONG_MIN when empty. */
long long max_amount(const Transaction* begin, const Transaction* end);

/** Number of transactions in [begin, end). */
size_t    count(const Transaction* begin, const Transaction* end);

/* ── Convenience overloads accepting a vector slice ──────────────── */

inline long long sum(const std::vector<Transaction>& v)
    { return sum(v.data(), v.data() + v.size()); }

inline double avg(const std::vector<Transaction>& v)
    { return avg(v.data(), v.data() + v.size()); }

inline long long max_amount(const std::vector<Transaction>& v)
    { return max_amount(v.data(), v.data() + v.size()); }

inline size_t count(const std::vector<Transaction>& v)
    { return v.size(); }

/* ── Group-by aggregates ─────────────────────────────────────────── */

/**
 * Aggregate total amount (cents) grouped by merchant_category.
 * Returns a map: category_string → total_cents.
 */
std::unordered_map<std::string, long long>
group_by_category(const Transaction* begin, const Transaction* end);

/**
 * Aggregate total amount (cents) grouped by US state code.
 * Returns a map: state_code → total_cents.
 */
std::unordered_map<std::string, long long>
group_by_state(const Transaction* begin, const Transaction* end);

/* ── Reduction helpers for parallel merge ────────────────────────── */

/**
 * Merge @p src into @p dst by summing values for matching keys.
 * Used to combine per-thread or per-rank partial group-by maps.
 */
void merge_group(std::unordered_map<std::string, long long>& dst,
                 const std::unordered_map<std::string, long long>& src);

/* ── Full sequential analytics (used as baseline) ───────────────── */

struct AnalyticsResult {
    long long total_cents;
    double    average_cents;
    long long max_cents;
    size_t    record_count;
    std::unordered_map<std::string, long long> by_category;
    std::unordered_map<std::string, long long> by_state;
};

/** Run all analytics sequentially on the full dataset. */
AnalyticsResult run_sequential(const std::vector<Transaction>& records);

} // namespace Analytics
