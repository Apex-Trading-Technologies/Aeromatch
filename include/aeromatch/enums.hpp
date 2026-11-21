#pragma once

#include <cstdint>

namespace aeromatch {

// ----------------------------------------------------------------------------
// Core Order Enums
// ----------------------------------------------------------------------------
enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    IOC = 2,
    FOK = 3
};

enum class OrderAcceptance : uint8_t {
    ACCEPTED = 0,
    ACCEPTED_ZERO_FILL = 1,
    REJECTED_DUPLICATE_ID = 2,
    REJECTED_INVALID_INPUT = 3,
    REJECTED_RATE_LIMITED = 4,
    REJECTED_FOK_UNFILLABLE = 5,
    REJECTED_NULL_POINTER = 6
};

// ----------------------------------------------------------------------------
// Market Taxonomy Enums
// ----------------------------------------------------------------------------
enum class AssetClass : uint8_t {
    EQUITY = 0,
    CRYPTO = 1,
    FOREX = 2,
    COMMODITY = 3,
    DERIVATIVE = 4
};

enum class Currency : uint8_t {
    USD = 0,
    EUR = 1,
    GBP = 2,
    INR = 3,
    JPY = 4,
    USDT = 5,
    USDC = 6,
    BTC = 7,
    ETH = 8
};

} // namespace aeromatch
