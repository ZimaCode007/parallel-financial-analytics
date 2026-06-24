#pragma once

#include <string>
#include <vector>
#include "performance.h"

/**
 * 结果文件读写模块
 *
 * 将单次运行的 RunStats 序列化为纯文本文件（.result），
 * 以及从多个 .result 文件反序列化后合并为完整的 stats 向量。
 *
 * 文件格式（逐行 key=value）：
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
 * 将单次运行结果保存到文本文件。
 *
 * @param file_path  输出文件路径（如 "results/seq.result"）
 * @param stats      待保存的运行统计
 */
void save_run_stats(const std::string& file_path, const RunStats& stats);

/**
 * 从文本文件加载单次运行结果。
 *
 * @param file_path  输入文件路径
 * @return           解析后的 RunStats
 * @throws std::runtime_error  文件无法打开时抛出
 */
RunStats load_run_stats(const std::string& file_path);
