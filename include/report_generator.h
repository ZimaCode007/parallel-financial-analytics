#pragma once

#include <string>
#include <vector>
#include "performance.h"

/**
 * HTML report generation module
 *
 * Exports analytics results and performance data as a self-contained static
 * HTML file with inline CSS and JavaScript — no network required to view it.
 *
 * The report includes:
 *   - Dataset overview cards (total records, total amount, average, maximum)
 *   - Performance comparison bar chart (execution time, speedup, efficiency)
 *   - Top-10 merchant categories horizontal bar chart
 *   - Top-10 US states horizontal bar chart
 */

/**
 * Generate a self-contained HTML report and write it to the given path.
 *
 * @param file_path  Output HTML file path (e.g. "report.html").
 * @param stats      Performance statistics for all runs (first element = sequential baseline).
 * @param csv_path   Original dataset file path (used for the report title only).
 */
void generate_html_report(const std::string& file_path,
                          const std::vector<RunStats>& stats,
                          const std::string& csv_path);
