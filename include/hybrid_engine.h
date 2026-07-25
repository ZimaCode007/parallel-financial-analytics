#pragma once

#include <vector>

#include "analytics.h"
#include "transaction.h"

/** Two-level MPI + OpenMP analytics engine. */
class HybridEngine {
public:
    /** @param omp_threads Threads per MPI process; 0 lets OpenMP decide. */
    explicit HybridEngine(int omp_threads = 0);

    /** Called by every MPI rank; records is required only on rank 0. */
    Analytics::AnalyticsResult run(const std::vector<Transaction>& records) const;

    bool is_root() const { return rank_ == 0; }
    int world_size() const { return world_size_; }
    int omp_threads() const { return omp_threads_; }

private:
    int rank_;
    int world_size_;
    int omp_threads_;
};
