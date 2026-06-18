#include "omp_engine.h"

#include <omp.h>
#include <climits>
#include "data_partitioner.h"

OmpEngine::OmpEngine(int num_threads) : num_threads_(num_threads) {
    if (num_threads_ > 0) {
        omp_set_num_threads(num_threads_);
    } else {
        // Read back the value that OpenMP has decided on.
        num_threads_ = omp_get_max_threads();
    }
}

Analytics::AnalyticsResult OmpEngine::run(const std::vector<Transaction>& records) const {
    const size_t n = records.size();
    if (n == 0) return {};

    // Obtain per-thread index ranges without copying any data.
    auto ranges = DataPartitioner::partition_ranges(n, static_cast<size_t>(num_threads_));
    const int num_parts = static_cast<int>(ranges.size());

    /* Scalars reduced across threads */
    long long global_sum   = 0;
    long long global_max   = LLONG_MIN;
    size_t    global_count = 0;

    /* Per-thread group-by maps (built privately, merged afterwards) */
    std::vector<std::unordered_map<std::string, long long>> partial_by_cat(num_parts);
    std::vector<std::unordered_map<std::string, long long>> partial_by_state(num_parts);

    #pragma omp parallel num_threads(num_parts)
    {
        int tid = omp_get_thread_num();
        auto [start, end] = ranges[tid];

        const Transaction* b = records.data() + start;
        const Transaction* e = records.data() + end;

        // Compute local aggregates.
        long long local_sum = Analytics::sum(b, e);
        long long local_max = Analytics::max_amount(b, e);
        size_t    local_cnt = static_cast<size_t>(e - b);

        partial_by_cat[tid]   = Analytics::group_by_category(b, e);
        partial_by_state[tid] = Analytics::group_by_state(b, e);

        // Reduce scalars with a critical section to avoid data races.
        // (OpenMP reduction clauses cannot be used here because the ranges
        //  are chosen dynamically; a critical section is the portable option.)
        #pragma omp critical
        {
            global_sum   += local_sum;
            global_count += local_cnt;
            if (local_max > global_max) global_max = local_max;
        }
    } // end parallel

    // Serial merge of group-by maps (small relative to analytics work).
    Analytics::AnalyticsResult result;
    result.record_count  = global_count;
    result.total_cents   = global_sum;
    result.average_cents = (global_count > 0)
                           ? static_cast<double>(global_sum) / static_cast<double>(global_count)
                           : 0.0;
    result.max_cents     = global_max;

    for (int i = 0; i < num_parts; ++i) {
        Analytics::merge_group(result.by_category, partial_by_cat[i]);
        Analytics::merge_group(result.by_state,    partial_by_state[i]);
    }

    return result;
}
