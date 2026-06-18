#include "data_loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

// Expected CSV column indices for the IBM Credit Card Transactions Dataset.
// Adjust these constants if the column layout of the CSV changes.
static constexpr int COL_ID       = 0;
static constexpr int COL_DATE     = 1;
static constexpr int COL_AMOUNT   = 2;
static constexpr int COL_MERCHANT = 3;
static constexpr int COL_CATEGORY = 4;
static constexpr int COL_STATE    = 5;
static constexpr int COL_CARD     = 6;
static constexpr int MIN_COLS     = 7;

DataLoader::DataLoader(const std::string& file_path)
    : file_path_(file_path) {}

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

// Split a CSV line into fields. Handles fields enclosed in double-quotes
// (including quoted fields that contain commas).
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

// Convert a dollar string ("$12.34", "12.34", "-$5.00") to integer cents.
// The IBM dataset uses a leading '$' sign and two decimal places.
long long DataLoader::parse_amount_cents(const std::string& raw) {
    std::string s = raw;
    // Strip leading/trailing whitespace and currency symbol.
    while (!s.empty() && (s.front() == ' ' || s.front() == '$')) s.erase(s.begin());
    while (!s.empty() && s.back() == ' ') s.pop_back();

    if (s.empty()) return 0;

    bool negative = false;
    if (s.front() == '-') { negative = true; s.erase(s.begin()); }

    // Find the decimal point.
    auto dot = s.find('.');
    long long dollars = 0;
    long long cents   = 0;

    if (dot == std::string::npos) {
        dollars = std::stoll(s);
    } else {
        dollars = std::stoll(s.substr(0, dot));
        std::string frac = s.substr(dot + 1);
        // Normalise fractional part to exactly 2 digits.
        while (frac.size() < 2) frac += '0';
        if (frac.size() > 2) frac = frac.substr(0, 2);
        cents = std::stoll(frac);
    }

    long long result = dollars * 100 + cents;
    return negative ? -result : result;
}
