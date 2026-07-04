#pragma once

#include <string>
#include <vector>
#include "performance.h"

/**
 * Result file I/O module
 *
 * Serialises a single RunStats object to a plain-text .result file and
 * deserialises it back.  Multiple .result files can be loaded and combined
 * into a complete stats vector for report generation.
 *
 * File format (one key=value per line):
 *   label=OpenMP-4
 *   elapsed_ms=109.60
 *   parallelism=4
 *   record_count=1296675
 *   total_cents=9122242890
 *   average_cents=7035.12
 *   max_cents=2894890
 *   cat:grocery_pos=1446082238
 *   state:TX=680091753
 *   ...
 */

/**
 * Save a single run's results to a text file.
 *
 * @param file_path  Output file path (e.g. "results/seq.result").
 * @param stats      Run statistics to save.
 */
void save_run_stats(const std::string& file_path, const RunStats& stats);

/**
 * Load a single run's results from a text file.
 *
 * @param file_path  Input file path.
 * @return           Parsed RunStats.
 * @throws std::runtime_error  If the file cannot be opened.
 */
RunStats load_run_stats(const std::string& file_path);
