#pragma once

#include <chrono>
#include <string>
#include "analytics.h"

/**
 * Performance Evaluation Module
 *
 * Provides timing primitives and a reporting helper that prints a
 * structured table of execution times, speedup, and efficiency for
 * sequential, OpenMP, and MPI runs.
 *
 * Speedup  S(p) = T_seq / T_par
 * Efficiency E(p) = S(p) / p         (p = number of threads/processes)
 */

/* ── Scoped wall-clock timer ─────────────────────────────────────── */

class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    /** Restart the timer. */
    void reset() { start_ = std::chrono::high_resolution_clock::now(); }

    /** Elapsed time in seconds since construction or last reset(). */
    double elapsed_s() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(now - start_).count();
    }

    /** Elapsed time in milliseconds. */
    double elapsed_ms() const { return elapsed_s() * 1000.0; }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

/* ── Result record for one run ───────────────────────────────────── */

struct RunStats {
    std::string label;          // e.g. "Sequential", "OpenMP-4", "MPI-8"
    double      elapsed_ms;     // Wall-clock time in milliseconds
    int         parallelism;    // Number of threads or MPI ranks (1 for sequential)
    Analytics::AnalyticsResult result;
};

/* ── Reporting ───────────────────────────────────────────────────── */

/**
 * Print a formatted performance comparison table to stdout.
 * The first RunStats entry is treated as the sequential baseline for
 * speedup and efficiency calculations.
 */
void print_performance_report(const std::vector<RunStats>& stats);

/**
 * Print the top-N categories and states from an AnalyticsResult.
 * Useful for a quick sanity-check that all engines produce the same output.
 */
void print_analytics_summary(const Analytics::AnalyticsResult& result,
                              size_t top_n = 5);
