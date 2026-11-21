#pragma once

#include <cstddef>
#include <cstdint>

namespace aeromatch {
namespace config {

/// Fixed symbol length used across the engine (16 bytes)
static constexpr size_t SYMBOL_LEN = 16;

/// Maximum number of distinct symbols this engine instance supports.
static constexpr size_t MAX_SYMBOLS = 1024;

/// Maximum resting orders per symbol (applies to resting LIMIT orders).
static constexpr uint32_t MAX_ACTIVE_ORDERS_PER_SYMBOL = 1'000'000;

/// Maximum resting orders per user per symbol (anti-DoS bound).
static constexpr uint32_t MAX_ACTIVE_ORDERS_PER_USER = 100'000;

/// Default pre-allocated Order pool capacity.
static constexpr size_t OBJECT_POOL_CAPACITY = 1'000'000;

/// Lock-free SPSC Ring buffer capacity (must be a power of two).
static constexpr size_t RING_BUFFER_CAPACITY = 65'536;

/// Maximum iterations per match loop to avoid CPU starvation.
static constexpr uint32_t MAX_MATCH_ITERATIONS = 50'000;

} // namespace config
} // namespace aeromatch
