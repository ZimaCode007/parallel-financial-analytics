#pragma once

#include <string>

/**
 * Represents a single financial transaction record parsed from the IBM
 * Credit Card Transactions Dataset CSV file.
 *
 * Field names match the IBM dataset column conventions; amounts are stored
 * in cents (integer) to avoid floating-point accumulation errors during
 * large-scale parallel summation.
 */
struct Transaction {
    long long   id;               // Unique transaction identifier
    std::string date;             // Transaction date (YYYY-MM-DD)
    long long   amount_cents;     // Transaction amount in cents (e.g. $12.34 → 1234)
    std::string merchant_name;
    std::string merchant_category; // MCC category string (e.g. "Grocery Stores")
    std::string state;             // US state code (e.g. "CA")
    std::string card_last4;        // Last 4 digits of the card (anonymised)
};
