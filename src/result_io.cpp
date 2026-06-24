#include "result_io.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

/**
 * 将单次运行结果序列化为纯文本文件。
 *
 * 文件格式为逐行 key=value，group-by 条目用 cat: / state: 前缀区分。
 * 该格式可被 load_run_stats 反序列化。
 *
 * @param file_path  输出文件路径
 * @param stats      待保存的 RunStats 对象
 */
void save_run_stats(const std::string& file_path, const RunStats& stats) {
    std::ofstream out(file_path);
    if (!out.is_open()) {
        std::cerr << "[ResultIO] Cannot write to " << file_path << "\n";
        return;
    }

    out << "label=" << stats.label << "\n";
    out << "elapsed_ms=" << stats.elapsed_ms << "\n";
    out << "parallelism=" << stats.parallelism << "\n";
    out << "record_count=" << stats.result.record_count << "\n";
    out << "total_cents=" << stats.result.total_cents << "\n";
    out << "average_cents=" << stats.result.average_cents << "\n";
    out << "max_cents=" << stats.result.max_cents << "\n";

    for (const auto& [key, val] : stats.result.by_category) {
        out << "cat:" << key << "=" << val << "\n";
    }
    for (const auto& [key, val] : stats.result.by_state) {
        out << "state:" << key << "=" << val << "\n";
    }

    out.close();
    std::cout << "[ResultIO] Saved " << stats.label << " → " << file_path << "\n";
}

/**
 * 从纯文本文件反序列化单次运行结果。
 *
 * 逐行读取 key=value 对，根据 key 前缀分类填充 RunStats 各字段。
 * 未识别的 key 会被静默忽略。
 *
 * @param file_path  输入文件路径
 * @return           解析后的 RunStats 对象
 * @throws std::runtime_error  文件无法打开时抛出
 */
RunStats load_run_stats(const std::string& file_path) {
    std::ifstream in(file_path);
    if (!in.is_open()) {
        throw std::runtime_error("[ResultIO] Cannot open " + file_path);
    }

    RunStats stats;
    std::string line;

    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if      (key == "label")        stats.label = val;
        else if (key == "elapsed_ms")   stats.elapsed_ms = std::stod(val);
        else if (key == "parallelism")  stats.parallelism = std::stoi(val);
        else if (key == "record_count") stats.result.record_count = std::stoull(val);
        else if (key == "total_cents")  stats.result.total_cents = std::stoll(val);
        else if (key == "average_cents")stats.result.average_cents = std::stod(val);
        else if (key == "max_cents")    stats.result.max_cents = std::stoll(val);
        else if (key.rfind("cat:", 0) == 0) {
            stats.result.by_category[key.substr(4)] = std::stoll(val);
        }
        else if (key.rfind("state:", 0) == 0) {
            stats.result.by_state[key.substr(6)] = std::stoll(val);
        }
    }

    return stats;
}
