#pragma once

#include <unordered_map>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>
#include <vector>
#include <memory>
#include "config.hpp"
#include "order.hpp"
#include "trade.hpp"
#include "order_book.hpp"
#include "event_tap.hpp"
#include "infra/ring_buffer.hpp"
#include "infra/object_pool.hpp"
#include "infra/metrics.hpp"

namespace aeromatch {

class MatchingEngine {
public:
    MatchingEngine(Metrics& metrics,
                   ObjectPool<Order>& order_pool,
                   ITradeEventTap* event_tap = nullptr,
                   uint32_t max_symbols = config::MAX_SYMBOLS,
                   uint32_t max_active_orders_per_symbol = config::MAX_ACTIVE_ORDERS_PER_SYMBOL,
                   uint32_t max_active_orders_per_user = config::MAX_ACTIVE_ORDERS_PER_USER);

    ~MatchingEngine();

    void start();
    void stop();
    void run();

    RingBuffer<Order, config::RING_BUFFER_CAPACITY>& get_ring_buffer() {
        return order_buffer_;
    }

    bool is_running() const { return running_.load(std::memory_order_acquire); }

    const OrderBook* get_book(const std::string& symbol) const;

    AddOrderResult process_order(Order&& order);

    // In-memory symbol management (zero external DB dependency)
    void register_symbol(const SymbolInfo& info);
    bool has_symbol(const std::string& symbol) const;
    const SymbolInfo* get_symbol(const std::string& symbol) const;

    // Observability getters
    size_t active_order_count() const;
    size_t pool_used() const;
    size_t pool_capacity() const;

private:
    Metrics& metrics_;
    ObjectPool<Order>& order_pool_;
    ITradeEventTap* event_tap_{nullptr};
    NullEventTap default_tap_;

    uint32_t max_symbols_;
    uint32_t max_active_orders_per_symbol_;
    uint32_t max_active_orders_per_user_;

    RingBuffer<Order, config::RING_BUFFER_CAPACITY> order_buffer_;
    std::unordered_map<std::string, OrderBook> books_;
    std::unordered_map<std::string, SymbolInfo> symbols_;
    mutable std::mutex symbols_mutex_;

    std::atomic<bool> running_{false};
    std::thread engine_thread_;
    mutable std::mutex books_mutex_;

    OrderBook* get_or_create_book(const std::string& symbol);
    void register_default_symbols();
};

} // namespace aeromatch
