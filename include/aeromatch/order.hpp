#pragma once

#include <cstdint>
#include <array>
#include <cstring>
#include <string>
#include <algorithm>
#include "config.hpp"
#include "enums.hpp"

namespace aeromatch {

// ----------------------------------------------------------------------------
// SymbolInfo structure – used by in-memory SymbolRegistry
// ----------------------------------------------------------------------------
struct SymbolInfo {
    std::string symbol;
    AssetClass asset_class{AssetClass::EQUITY};
    Currency currency{Currency::USD};
    int64_t tick_size{1};
    int64_t lot_size{1};
    int64_t min_price{1};
    int64_t max_price{100'000'000'000LL};
};

// ----------------------------------------------------------------------------
// Order struct – 64-byte cache-line aligned for ultra-low-latency traversal
// ----------------------------------------------------------------------------
struct alignas(64) Order {
    uint64_t order_id{0};
    std::array<unsigned char, 16> user_id{};
    std::array<char, config::SYMBOL_LEN> symbol{};
    Side side{Side::BUY};
    OrderType type{OrderType::LIMIT};
    int64_t price{0};
    int64_t quantity{0};
    int64_t remaining_quantity{0};
    uint64_t timestamp_ns{0};
    Order* next{nullptr};
    Order* prev{nullptr};

    AssetClass asset_class{AssetClass::EQUITY};
    Currency currency{Currency::USD};

    Order() = default;

    // Standard constructor without user_id
    Order(uint64_t id, const char* sym, Side s, OrderType t, int64_t p, int64_t q)
        : order_id(id), side(s), type(t), price(p), quantity(q), remaining_quantity(q) {
        if (sym) {
            std::strncpy(symbol.data(), sym, config::SYMBOL_LEN - 1);
            symbol[config::SYMBOL_LEN - 1] = '\0';
        }
    }

    // Constructor with user_id (16-byte UUID / identifier)
    Order(uint64_t id, const unsigned char* uuid, const char* sym, Side s, OrderType t, int64_t p, int64_t q)
        : order_id(id), side(s), type(t), price(p), quantity(q), remaining_quantity(q) {
        if (uuid) {
            std::memcpy(user_id.data(), uuid, 16);
        }
        if (sym) {
            std::strncpy(symbol.data(), sym, config::SYMBOL_LEN - 1);
            symbol[config::SYMBOL_LEN - 1] = '\0';
        }
    }
};

} // namespace aeromatch
