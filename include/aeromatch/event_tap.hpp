#pragma once

#include <cstdint>
#include "trade.hpp"
#include "order.hpp"

namespace aeromatch {

/// Pluggable interface for external consumers to receive execution events
/// without tightly coupling the core matching engine to any specific transport
/// (Kafka, ZeroMQ, WebSockets, gRPC, shared memory, or database).
class ITradeEventTap {
public:
    virtual ~ITradeEventTap() = default;
    virtual void on_trade(const Trade& trade) = 0;
    virtual void on_order_accepted(const Order& order) = 0;
    virtual void on_order_rejected(uint64_t order_id, uint8_t reason) = 0;
};

/// Default zero-cost no-op implementation.
class NullEventTap : public ITradeEventTap {
public:
    void on_trade(const Trade& /*trade*/) override {}
    void on_order_accepted(const Order& /*order*/) override {}
    void on_order_rejected(uint64_t /*order_id*/, uint8_t /*reason*/) override {}
};

} // namespace aeromatch
