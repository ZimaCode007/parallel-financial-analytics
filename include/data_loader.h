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
     * Randomly sample a given proportion of records from the loaded dataset.
     *
     * Uses the Fisher-Yates shuffle to randomise an index array and then
     * takes the first ratio*N entries, giving each record an equal probability
     * of being selected.
     *
     * @param records  Full dataset (the original vector is not modified).
     * @param ratio    Sampling ratio in (0.0, 1.0]; e.g. 0.5 means 50%.
     * @param seed     RNG seed — the same seed always produces the same sample.
     * @return         Sampled Transaction vector.
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
