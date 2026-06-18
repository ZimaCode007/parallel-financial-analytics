#pragma once

#include <vector>
#include "transaction.h"
#include "analytics.h"

/**
 * OpenMP Execution Module
 *
 * Implements the shared-memory parallel analytics pipeline using OpenMP.
 *
 * Strategy:
 *   - The dataset is split into index ranges (DataPartitioner::partition_ranges)
 *     so no data is copied – all threads read from the same vector in memory.
 *   - Basic aggregates (SUM, MAX, COUNT) use OpenMP reduction clauses for
 *     correctness without explicit locking.
 *   - Group-by maps are computed per-thread and merged afterwards via a
 *     critical section to avoid false sharing on the global map.
 *
 * Usage:
 *   OmpEngine engine(omp_get_max_threads());
 *   auto result = engine.run(records);
 */
class OmpEngine {
public:
    /** @param num_threads  Number of OpenMP threads to use (0 = system default). */
    explicit OmpEngine(int num_threads = 0);

    /** Run the full analytics suite in parallel and return aggregated results. */
    Analytics::AnalyticsResult run(const std::vector<Transaction>& records) const;

    int num_threads() const { return num_threads_; }

private:
    int num_threads_;
};
