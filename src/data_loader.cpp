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
 * Constructor: store the CSV file path; the file is not opened yet.
 *
 * @param file_path  Full path to the CSV file (e.g. "data/credit_card_transactions.csv").
 */
DataLoader::DataLoader(const std::string& file_path)
    : file_path_(file_path) {}

/**
 * Read and parse transaction records from the CSV file.
 *
 * Reads line by line, skips the header row and any row with too few fields,
 * and appends each successfully parsed line as a Transaction to the result.
 *
 * @param max_rows  Maximum rows to read; 0 means unlimited (load everything).
 * @return          Vector of parsed Transaction objects, shrink_to_fit applied.
 * @throws std::runtime_error  If the file cannot be opened.
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
 * Split a single CSV line into a vector of field strings.
 *
 * Supports double-quoted fields (which may contain commas).
 * Two consecutive double quotes inside a quoted field are treated as a
 * literal quote character.
 *
 * @param line  Raw CSV line (without trailing newline).
 * @return      Fields in order, split on unquoted commas.
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
 * Convert an amount string to integer cents.
 *
 * The dataset stores amounts as plain decimal strings without a currency
 * symbol (e.g. "4.97", "-5.00").  The value is rounded to the nearest cent
 * to avoid floating-point truncation errors.
 *
 * @param raw  Raw amount string, e.g. "107.23".
 * @return     Amount in cents (e.g. "107.23" → 10723); returns 0 for empty input.
 */
long long DataLoader::parse_amount_cents(const std::string& raw) {
    if (raw.empty()) return 0;
    // std::stod handles leading/trailing whitespace, sign, and scientific notation.
    double value = std::stod(raw);
    // Round to nearest cent to avoid floating-point truncation errors.
    return static_cast<long long>(value * 100.0 + (value >= 0 ? 0.5 : -0.5));
}

/**
 * Randomly sample a given proportion of records from the dataset.
 *
 * Applies Fisher-Yates shuffle to an index array, then takes the first
 * N*ratio indices.  A fixed seed guarantees reproducibility so the same
 * subset can be compared across different engines.
 *
 * @param records  Full dataset (read-only; original vector is not modified).
 * @param ratio    Sampling ratio in (0.0, 1.0]; e.g. 0.5 = random 50%.
 * @param seed     RNG seed (default 42).
 * @return         Sampled Transaction vector of size floor(records.size() * ratio).
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

    // Build an index array and shuffle it with Fisher-Yates.
    std::vector<size_t> indices(records.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 rng(seed);
    std::shuffle(indices.begin(), indices.end(), rng);

    // Collect the first sample_size indices.
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
