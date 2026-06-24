#pragma once

#include <string>
#include <vector>
#include "transaction.h"

/**
 * Data Loading Module
 *
 * Responsible for reading and preprocessing transaction records from a
 * CSV file on disk.  The loader performs:
 *   1. File I/O with line-by-line buffered reading
 *   2. Field splitting on comma (handles quoted fields)
 *   3. Type conversion (string → numeric) and amount normalisation to cents
 *   4. Optional row-limit for testing with sub-samples
 *
 * Usage:
 *   DataLoader loader("data/transactions.csv");
 *   auto records = loader.load();          // load all rows
 *   auto sample  = loader.load(100000);    // load first 100 k rows only
 */
class DataLoader {
public:
    explicit DataLoader(const std::string& file_path);

    /**
     * Parse CSV and return a vector of Transaction objects.
     * @param max_rows  0 = unlimited; positive value caps the number of rows loaded.
     */
    std::vector<Transaction> load(size_t max_rows = 0) const;

    /** Return the path this loader was constructed with. */
    const std::string& path() const { return file_path_; }

    /**
     * 从已加载的数据集中随机采样指定比例的记录。
     *
     * 使用 Fisher-Yates 洗牌算法随机打乱后截取前 ratio*N 条，
     * 保证每条记录被选中的概率相等。
     *
     * @param records  完整数据集（不修改原向量）
     * @param ratio    采样比例，取值 (0.0, 1.0]；如 0.5 表示 50%
     * @param seed     随机种子，相同种子产生相同采样结果（可复现）
     * @return         采样后的 Transaction 向量
     */
    static std::vector<Transaction> sample(
        const std::vector<Transaction>& records,
        double ratio,
        unsigned int seed = 42);

private:
    std::string file_path_;

    /** Split a single CSV line into fields, respecting double-quoted values. */
    static std::vector<std::string> split_csv_line(const std::string& line);

    /** Convert a dollar-string like "$12.34" or "12.34" to integer cents. */
    static long long parse_amount_cents(const std::string& raw);
};
