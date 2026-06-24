#include "report_generator.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <ctime>

/**
 * 将整数分转为美元字符串（如 10723 → "107.23"）。
 *
 * @param cents  金额，单位为分
 * @return       不含 $ 符号的数字字符串
 */
static std::string cents_to_dollars(long long cents) {
    long long abs_c = cents < 0 ? -cents : cents;
    std::string s = std::to_string(abs_c / 100) + "." +
                    (abs_c % 100 < 10 ? "0" : "") +
                    std::to_string(abs_c % 100);
    return (cents < 0 ? "-" : "") + s;
}

/**
 * 对 group-by map 按值降序排序，返回前 n 个键值对。
 *
 * @param m  原始分组统计 map
 * @param n  返回的最大条数
 * @return   按金额降序排列的 (键, 值) 向量
 */
static std::vector<std::pair<std::string, long long>>
top_n(const std::unordered_map<std::string, long long>& m, size_t n) {
    std::vector<std::pair<std::string, long long>> v(m.begin(), m.end());
    std::sort(v.begin(), v.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.size() > n) v.resize(n);
    return v;
}

/**
 * 对字符串进行 HTML 转义，防止特殊字符破坏 HTML 结构。
 *
 * @param s  原始字符串
 * @return   转义后的安全字符串（& < > " ' 被替换为实体）
 */
static std::string html_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }
    return out;
}

/**
 * 生成自包含 HTML 报告并写入指定路径。
 *
 * 报告包含四个可视化区域：
 *   1. 概览卡片：记录数、总金额、平均值、最大值
 *   2. 性能对比：执行时间柱状图 + 加速比/效率数值
 *   3. Top-10 商户类别水平条形图
 *   4. Top-10 美国州水平条形图
 *
 * 所有 CSS、JavaScript 和数据均内嵌在单个 HTML 文件中，
 * 无需网络连接即可在任何浏览器中打开。
 *
 * @param file_path  输出文件路径
 * @param stats      RunStats 向量（stats[0] 必须是顺序基准）
 * @param csv_path   数据集路径（仅用于页面标题展示）
 */
void generate_html_report(const std::string& file_path,
                          const std::vector<RunStats>& stats,
                          const std::string& csv_path) {
    if (stats.empty()) return;

    const auto& baseline = stats[0].result;
    double seq_ms = stats[0].elapsed_ms;

    // Get current timestamp for the report header.
    auto now = std::time(nullptr);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    // Prepare top-10 group-by data from the baseline (all engines produce the same result).
    auto top_cats   = top_n(baseline.by_category, 10);
    auto top_states = top_n(baseline.by_state, 10);

    std::ofstream out(file_path);
    if (!out.is_open()) {
        std::cerr << "[ReportGenerator] Cannot write to " << file_path << "\n";
        return;
    }

    out << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Parallel Financial Analytics Engine — Report</title>
<style>
  :root {
    --bg: #0f172a; --surface: #1e293b; --surface2: #334155;
    --text: #f1f5f9; --text2: #94a3b8; --accent: #38bdf8;
    --green: #4ade80; --orange: #fb923c; --pink: #f472b6;
    --bar-seq: #38bdf8; --bar-omp: #4ade80; --bar-mpi: #fb923c;
    --radius: 12px;
  }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: var(--bg); color: var(--text); padding: 32px; line-height: 1.6;
  }
  h1 { font-size: 1.8rem; margin-bottom: 4px; }
  .subtitle { color: var(--text2); font-size: 0.9rem; margin-bottom: 32px; }
  .section-title { font-size: 1.2rem; margin: 32px 0 16px; color: var(--accent); }

  /* ── Cards ── */
  .cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; }
  .card {
    background: var(--surface); border-radius: var(--radius); padding: 20px;
    border: 1px solid var(--surface2);
  }
  .card .label { color: var(--text2); font-size: 0.8rem; text-transform: uppercase; letter-spacing: 1px; }
  .card .value { font-size: 1.6rem; font-weight: 700; margin-top: 4px; }

  /* ── Performance table ── */
  .perf-table { width: 100%; border-collapse: collapse; margin-top: 16px; }
  .perf-table th, .perf-table td {
    padding: 12px 16px; text-align: left; border-bottom: 1px solid var(--surface2);
  }
  .perf-table th { color: var(--text2); font-size: 0.8rem; text-transform: uppercase; letter-spacing: 1px; }
  .perf-table tr:hover { background: var(--surface); }

  /* ── Bar chart (horizontal) ── */
  .bar-chart { margin-top: 8px; }
  .bar-row { display: flex; align-items: center; margin-bottom: 8px; }
  .bar-label { width: 180px; font-size: 0.85rem; color: var(--text2); flex-shrink: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .bar-track { flex: 1; height: 28px; background: var(--surface2); border-radius: 6px; overflow: hidden; position: relative; }
  .bar-fill { height: 100%; border-radius: 6px; display: flex; align-items: center; padding-left: 10px;
    font-size: 0.75rem; font-weight: 600; color: var(--bg); transition: width 0.6s ease; min-width: 40px; }
  .bar-value { margin-left: 12px; font-size: 0.8rem; color: var(--text2); width: 100px; text-align: right; flex-shrink: 0; }

  /* ── Performance bars (vertical) ── */
  .perf-bars { display: flex; justify-content: center; gap: 40px; margin: 24px 0; align-items: flex-end; height: 260px; }
  .perf-col { display: flex; flex-direction: column; align-items: center; width: 100px; }
  .perf-bar-wrap { width: 60px; height: 220px; background: var(--surface2); border-radius: 8px; overflow: hidden;
    display: flex; flex-direction: column; justify-content: flex-end; position: relative; }
  .perf-bar { border-radius: 8px 8px 0 0; transition: height 0.8s ease; display: flex; align-items: flex-end; justify-content: center; padding-bottom: 8px;
    font-size: 0.7rem; font-weight: 700; color: var(--bg); }
  .perf-label { margin-top: 10px; font-size: 0.8rem; color: var(--text2); text-align: center; }

  /* ── Footer ── */
  .footer { margin-top: 48px; padding-top: 24px; border-top: 1px solid var(--surface2); color: var(--text2); font-size: 0.8rem; text-align: center; }

  /* ── Metric badges ── */
  .metrics { display: flex; gap: 16px; margin-top: 12px; flex-wrap: wrap; }
  .metric { background: var(--surface); border-radius: 8px; padding: 10px 16px; border: 1px solid var(--surface2); }
  .metric .m-label { font-size: 0.7rem; color: var(--text2); text-transform: uppercase; }
  .metric .m-value { font-size: 1.1rem; font-weight: 700; }
</style>
</head>
<body>

<h1>Parallel Financial Analytics Engine</h1>
<p class="subtitle">Report generated )" << time_buf << " &mdash; Dataset: " << html_escape(csv_path) << R"(</p>

<!-- ── Overview Cards ── -->
<h2 class="section-title">Dataset Overview</h2>
<div class="cards">
  <div class="card">
    <div class="label">Total Records</div>
    <div class="value">)" << baseline.record_count << R"(</div>
  </div>
  <div class="card">
    <div class="label">Total Amount</div>
    <div class="value">$)" << cents_to_dollars(baseline.total_cents) << R"(</div>
  </div>
  <div class="card">
    <div class="label">Average Amount</div>
    <div class="value">$)" << cents_to_dollars(static_cast<long long>(baseline.average_cents)) << R"(</div>
  </div>
  <div class="card">
    <div class="label">Maximum Amount</div>
    <div class="value">$)" << cents_to_dollars(baseline.max_cents) << R"(</div>
  </div>
</div>

<!-- ── Performance Comparison ── -->
<h2 class="section-title">Performance Comparison</h2>
<div class="perf-bars">
)";

    // Find max time for scaling bars.
    double max_ms = 0;
    for (const auto& s : stats) {
        if (s.elapsed_ms > max_ms) max_ms = s.elapsed_ms;
    }

    const char* colors[] = {"var(--bar-seq)", "var(--bar-omp)", "var(--bar-mpi)",
                             "var(--pink)", "var(--accent)"};
    int ci = 0;
    for (const auto& s : stats) {
        double pct = (s.elapsed_ms / max_ms) * 100.0;
        out << "  <div class=\"perf-col\">\n"
            << "    <div class=\"perf-bar-wrap\">\n"
            << "      <div class=\"perf-bar\" style=\"height:" << pct
            << "%; background:" << colors[ci % 5] << ";\">"
            << std::fixed << std::setprecision(1) << s.elapsed_ms << "ms</div>\n"
            << "    </div>\n"
            << "    <div class=\"perf-label\">" << html_escape(s.label) << "</div>\n"
            << "  </div>\n";
        ++ci;
    }

    out << R"(</div>

<!-- Speedup & Efficiency metrics -->
<div class="metrics">
)";

    for (size_t i = 1; i < stats.size(); ++i) {
        double speedup    = seq_ms / stats[i].elapsed_ms;
        double efficiency = speedup / static_cast<double>(stats[i].parallelism);
        out << "  <div class=\"metric\">\n"
            << "    <div class=\"m-label\">" << html_escape(stats[i].label) << " Speedup</div>\n"
            << "    <div class=\"m-value\">" << std::fixed << std::setprecision(2) << speedup << "x</div>\n"
            << "  </div>\n"
            << "  <div class=\"metric\">\n"
            << "    <div class=\"m-label\">" << html_escape(stats[i].label) << " Efficiency</div>\n"
            << "    <div class=\"m-value\">" << std::fixed << std::setprecision(1) << (efficiency * 100) << "%</div>\n"
            << "  </div>\n";
    }

    out << R"(</div>

<!-- Performance Detail Table -->
<table class="perf-table">
  <thead>
    <tr><th>Run</th><th>Time (ms)</th><th>Parallelism</th><th>Speedup</th><th>Efficiency</th></tr>
  </thead>
  <tbody>
)";

    for (const auto& s : stats) {
        double speedup    = seq_ms / s.elapsed_ms;
        double efficiency = speedup / static_cast<double>(s.parallelism);
        out << "    <tr><td>" << html_escape(s.label) << "</td>"
            << "<td>" << std::fixed << std::setprecision(2) << s.elapsed_ms << "</td>"
            << "<td>" << s.parallelism << "</td>"
            << "<td>" << std::setprecision(2) << speedup << "x</td>"
            << "<td>" << std::setprecision(1) << (efficiency * 100) << "%</td></tr>\n";
    }

    out << R"(  </tbody>
</table>

<!-- ── Top Categories ── -->
<h2 class="section-title">Top 10 Merchant Categories by Transaction Amount</h2>
<div class="bar-chart">
)";

    long long cat_max = top_cats.empty() ? 1 : top_cats[0].second;
    for (const auto& [name, val] : top_cats) {
        double pct = static_cast<double>(val) / static_cast<double>(cat_max) * 100.0;
        out << "  <div class=\"bar-row\">\n"
            << "    <div class=\"bar-label\">" << html_escape(name) << "</div>\n"
            << "    <div class=\"bar-track\"><div class=\"bar-fill\" style=\"width:"
            << pct << "%; background: var(--accent);\"></div></div>\n"
            << "    <div class=\"bar-value\">$" << cents_to_dollars(val) << "</div>\n"
            << "  </div>\n";
    }

    out << R"(</div>

<!-- ── Top States ── -->
<h2 class="section-title">Top 10 States by Transaction Amount</h2>
<div class="bar-chart">
)";

    long long state_max = top_states.empty() ? 1 : top_states[0].second;
    for (const auto& [name, val] : top_states) {
        double pct = static_cast<double>(val) / static_cast<double>(state_max) * 100.0;
        out << "  <div class=\"bar-row\">\n"
            << "    <div class=\"bar-label\">" << html_escape(name) << "</div>\n"
            << "    <div class=\"bar-track\"><div class=\"bar-fill\" style=\"width:"
            << pct << "%; background: var(--green);\"></div></div>\n"
            << "    <div class=\"bar-value\">$" << cents_to_dollars(val) << "</div>\n"
            << "  </div>\n";
    }

    out << R"(</div>

<div class="footer">
  Parallel Financial Analytics Engine &mdash; Nguyen Phu Pham &middot; Roshika Pant &middot; Bingqian Yang
</div>

</body>
</html>
)";

    out.close();
    std::cout << "[ReportGenerator] HTML report written to " << file_path << "\n";
}
