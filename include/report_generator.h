#pragma once

#include <string>
#include <vector>
#include "performance.h"

/**
 * HTML 报告生成模块
 *
 * 将分析结果和性能数据导出为一个自包含的静态 HTML 文件，
 * 内嵌 CSS 和 JavaScript，无需网络即可在浏览器中查看。
 *
 * 报告包含：
 *   - 数据集概览卡片（总记录数、总金额、平均值、最大值）
 *   - 性能对比柱状图（执行时间、加速比、效率）
 *   - 按商户类别分组的 Top-10 水平条形图
 *   - 按美国州分组的 Top-10 水平条形图
 */

/**
 * 生成自包含 HTML 报告并写入指定路径。
 *
 * @param file_path  输出 HTML 文件路径（如 "report.html"）
 * @param stats      所有运行的性能统计数据（首元素为顺序基准）
 * @param csv_path   原始数据集文件路径（用于报告标题展示）
 */
void generate_html_report(const std::string& file_path,
                          const std::vector<RunStats>& stats,
                          const std::string& csv_path);
