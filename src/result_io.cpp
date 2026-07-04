#include "result_io.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

/**
 * Serialise a single run's results to a plain-text file.
 *
 * Format: one key=value pair per line; group-by entries use a cat: or state: prefix.
 * The file can be read back by load_run_stats.
 *
 * @param file_path  Output file path.
 * @param stats      RunStats object to save.
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
 * Deserialise a single run's results from a plain-text file.
 *
 * Reads key=value pairs line by line and fills in RunStats fields by key prefix.
 * Unrecognised keys are silently ignored.
 *
 * @param file_path  Input file path.
 * @return           Parsed RunStats object.
 * @throws std::runtime_error  If the file cannot be opened.
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
