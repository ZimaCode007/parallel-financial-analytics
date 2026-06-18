#include "performance.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

void print_performance_report(const std::vector<RunStats>& stats) {
    if (stats.empty()) return;

    double seq_ms = stats[0].elapsed_ms;

    std::cout << "\n";
    std::cout << "=================================================================\n";
    std::cout << "  Parallel Financial Analytics Engine — Performance Report\n";
    std::cout << "=================================================================\n";
    std::cout << std::left
              << std::setw(18) << "Run"
              << std::setw(14) << "Time (ms)"
              << std::setw(12) << "Speedup"
              << std::setw(12) << "Efficiency"
              << "\n";
    std::cout << "-----------------------------------------------------------------\n";

    for (const auto& s : stats) {
        double speedup    = seq_ms / s.elapsed_ms;
        double efficiency = speedup / static_cast<double>(s.parallelism);

        std::cout << std::left  << std::setw(18) << s.label
                  << std::right << std::setw(10) << std::fixed << std::setprecision(2)
                  << s.elapsed_ms << "    "
                  << std::setw(8) << speedup << "    "
                  << std::setw(8) << efficiency
                  << "\n";
    }

    std::cout << "=================================================================\n\n";
}

void print_analytics_summary(const Analytics::AnalyticsResult& result, size_t top_n) {
    auto format_dollars = [](long long cents) -> std::string {
        long long abs_cents = cents < 0 ? -cents : cents;
        std::string s = std::to_string(abs_cents / 100) + "." +
                        (abs_cents % 100 < 10 ? "0" : "") +
                        std::to_string(abs_cents % 100);
        return (cents < 0 ? "-$" : "$") + s;
    };

    std::cout << "--- Analytics Summary ---\n";
    std::cout << "  Record count : " << result.record_count << "\n";
    std::cout << "  Total amount : " << format_dollars(result.total_cents) << "\n";
    std::cout << "  Average      : " << format_dollars(static_cast<long long>(result.average_cents)) << "\n";
    std::cout << "  Maximum      : " << format_dollars(result.max_cents) << "\n";

    // Sort group-by maps by descending total and print top-N.
    auto print_top = [&](const std::unordered_map<std::string, long long>& m,
                         const std::string& label) {
        std::vector<std::pair<std::string, long long>> v(m.begin(), m.end());
        std::sort(v.begin(), v.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        std::cout << "  Top " << top_n << " by " << label << ":\n";
        for (size_t i = 0; i < std::min(top_n, v.size()); ++i) {
            std::cout << "    " << std::left << std::setw(30) << v[i].first
                      << format_dollars(v[i].second) << "\n";
        }
    };

    print_top(result.by_category, "category");
    print_top(result.by_state,    "state");
    std::cout << "-------------------------\n";
}
