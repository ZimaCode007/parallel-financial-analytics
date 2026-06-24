#include "data_loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <random>

// Column indices for credit_card_transactions.csv
// Header: Unnamed:0, trans_date_trans_time, cc_num, merchant, category,
//         amt, first, last, gender, street, city, state, zip, ...
static constexpr int COL_ID       = 0;
static constexpr int COL_DATE     = 1;
static constexpr int COL_CARD     = 2;
static constexpr int COL_MERCHANT = 3;
static constexpr int COL_CATEGORY = 4;
static constexpr int COL_AMOUNT   = 5;
static constexpr int COL_STATE    = 11;
static constexpr int MIN_COLS     = 12;  // minimum fields required per row

/**
 * 构造函数：记录 CSV 文件路径，不立即打开文件。
 *
 * @param file_path  CSV 文件的完整路径（如 "data/credit_card_transactions.csv"）
 */
DataLoader::DataLoader(const std::string& file_path)
    : file_path_(file_path) {}

/**
 * 从 CSV 文件读取并解析交易记录。
 *
 * 逐行读取文件，跳过首行表头和字段数不足的行，
 * 将每行解析为一个 Transaction 对象后追加到返回向量。
 *
 * @param max_rows  最多读取的行数；0 表示不限，读取全部数据。
 * @return          解析成功的 Transaction 对象向量，已 shrink_to_fit。
 * @throws std::runtime_error  文件无法打开时抛出。
 */
std::vector<Transaction> DataLoader::load(size_t max_rows) const {
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        throw std::runtime_error("DataLoader: cannot open file: " + file_path_);
    }

    std::vector<Transaction> records;
    // Pre-allocate to avoid repeated reallocation on large files.
    if (max_rows > 0) records.reserve(max_rows);
    else              records.reserve(5'000'000);

    std::string line;
    // Skip the header row.
    if (!std::getline(file, line)) return records;

    size_t row = 0;
    while (std::getline(file, line)) {
        if (max_rows > 0 && row >= max_rows) break;

        auto fields = split_csv_line(line);
        if (static_cast<int>(fields.size()) < MIN_COLS) {
            // Skip malformed rows silently to be robust against trailing newlines.
            continue;
        }

        Transaction t;
        try {
            t.id                = std::stoll(fields[COL_ID]);
            t.date              = fields[COL_DATE];
            t.amount_cents      = parse_amount_cents(fields[COL_AMOUNT]);
            t.merchant_name     = fields[COL_MERCHANT];
            t.merchant_category = fields[COL_CATEGORY];
            t.state             = fields[COL_STATE];
            t.card_last4        = fields[COL_CARD];
        } catch (...) {
            // Skip rows with unparseable numeric fields.
            continue;
        }

        records.push_back(std::move(t));
        ++row;
    }

    records.shrink_to_fit();
    std::cout << "[DataLoader] Loaded " << records.size()
              << " transactions from " << file_path_ << "\n";
    return records;
}

/**
 * 将单行 CSV 文本拆分为字段数组。
 *
 * 支持双引号包裹的字段（字段内可含逗号），
 * 连续两个双引号（""）被解释为一个字面双引号字符。
 *
 * @param line  待解析的 CSV 行字符串（不含换行符）
 * @return      按逗号分隔的字段字符串向量，顺序与原行一致
 */
std::vector<std::string> DataLoader::split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            // Two consecutive quotes inside a quoted field → literal quote.
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == ',' && !in_quotes) {
            fields.push_back(std::move(field));
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(std::move(field));
    return fields;
}

/**
 * 将金额字符串转换为整数分（cents）。
 *
 * 数据集中金额为无货币符号的十进制字符串（如 "4.97"、"-5.00"）。
 * 转换时四舍五入到最近的分，避免浮点截断误差。
 *
 * @param raw  原始金额字符串，如 "107.23"
 * @return     以分为单位的整数金额（如 "107.23" → 10723）；空字符串返回 0
 */
long long DataLoader::parse_amount_cents(const std::string& raw) {
    if (raw.empty()) return 0;
    // std::stod handles leading/trailing whitespace, sign, and scientific notation.
    double value = std::stod(raw);
    // Round to nearest cent to avoid floating-point truncation errors.
    return static_cast<long long>(value * 100.0 + (value >= 0 ? 0.5 : -0.5));
}

/**
 * 从数据集中随机采样指定比例的记录。
 *
 * 采用 Fisher-Yates 洗牌对索引随机排列后取前 N*ratio 条。
 * 固定 seed 保证可复现性，便于对比同一子集在不同引擎上的结果。
 *
 * @param records  完整数据集（只读，不修改原向量）
 * @param ratio    采样比例，取值 (0.0, 1.0]；如 0.5 表示随机取 50%
 * @param seed     随机数种子，默认 42
 * @return         采样后的 Transaction 向量，大小为 floor(records.size() * ratio)
 */
std::vector<Transaction> DataLoader::sample(
    const std::vector<Transaction>& records,
    double ratio,
    unsigned int seed)
{
    if (ratio <= 0.0) return {};
    if (ratio >= 1.0) return records;

    size_t sample_size = static_cast<size_t>(
        static_cast<double>(records.size()) * ratio);
    if (sample_size == 0) return {};

    // 生成索引数组并随机打乱
    std::vector<size_t> indices(records.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 rng(seed);
    std::shuffle(indices.begin(), indices.end(), rng);

    // 取前 sample_size 个索引对应的记录
    std::vector<Transaction> sampled;
    sampled.reserve(sample_size);
    for (size_t i = 0; i < sample_size; ++i) {
        sampled.push_back(records[indices[i]]);
    }

    std::cout << "[DataLoader] Sampled " << sampled.size()
              << " / " << records.size() << " records ("
              << static_cast<int>(ratio * 100) << "%)\n";
    return sampled;
}
