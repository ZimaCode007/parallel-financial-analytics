#include "performance.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

/**
 * 打印多次运行的性能对比表格。
 *
 * 以 stats[0] 作为顺序执行基准，计算每条记录相对于基准的
 * 加速比（Speedup）和效率（Efficiency）并格式化输出。
 *
 * Speedup   S(p) = T_sequential / T_parallel
 * Efficiency E(p) = S(p) / p        （p 为线程数或进程数）
 *
 * @param stats  RunStats 向量，首元素必须为顺序执行结果；
 *               向量为空时函数直接返回，不产生任何输出
 */
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

/**
 * 打印单次分析结果的摘要，包含基本统计量和 group-by Top-N 排名。
 *
 * 金额以美元格式（$X.XX）显示，group-by 结果按总额降序排列后取前 top_n 条。
 * 该函数仅用于结果展示，不修改 result 的任何字段。
 *
 * @param result  待展示的分析结果（AnalyticsResult）
 * @param top_n   group-by 排行中展示的最大条数，默认为 5；
 *                若分组数量少于 top_n，则全部展示
 */
void print_analytics_summary(const Analytics::AnalyticsResult& result, size_t top_n) {
    // Lambda：将整数分转为带符号的美元字符串，如 10723 → "$107.23"
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

    // Lambda：对给定 group-by map 按值降序排列并打印前 top_n 项
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
