#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>

namespace aeromatch {

struct MetricsSnapshot {
    uint64_t orders_received = 0;
    uint64_t orders_matched = 0;
    uint64_t orders_rejected = 0;
    uint64_t trades_executed = 0;
    uint64_t critical_errors = 0;
    size_t   buffer_usage = 0;
};

class Metrics {
public:
    void increment_orders_received(uint64_t count = 1) noexcept {
        orders_received_.fetch_add(count, std::memory_order_relaxed);
    }

    void increment_orders_matched(uint64_t count = 1) noexcept {
        orders_matched_.fetch_add(count, std::memory_order_relaxed);
    }

    void increment_orders_rejected(uint64_t count = 1) noexcept {
        orders_rejected_.fetch_add(count, std::memory_order_relaxed);
    }

    void increment_trades_executed(uint64_t count = 1) noexcept {
        trades_executed_.fetch_add(count, std::memory_order_relaxed);
    }

    void increment_critical_errors(uint64_t count = 1) noexcept {
        critical_errors_.fetch_add(count, std::memory_order_relaxed);
    }

    void set_buffer_usage(size_t usage) noexcept {
        buffer_usage_.store(usage, std::memory_order_relaxed);
    }

    MetricsSnapshot snapshot() const noexcept {
        return {
            .orders_received = orders_received_.load(std::memory_order_acquire),
            .orders_matched  = orders_matched_.load(std::memory_order_acquire),
            .orders_rejected = orders_rejected_.load(std::memory_order_acquire),
            .trades_executed = trades_executed_.load(std::memory_order_acquire),
            .critical_errors = critical_errors_.load(std::memory_order_acquire),
            .buffer_usage    = buffer_usage_.load(std::memory_order_acquire)
        };
    }

private:
    std::atomic<uint64_t> orders_received_{0};
    std::atomic<uint64_t> orders_matched_{0};
    std::atomic<uint64_t> orders_rejected_{0};
    std::atomic<uint64_t> trades_executed_{0};
    std::atomic<uint64_t> critical_errors_{0};
    std::atomic<size_t>   buffer_usage_{0};
};

} // namespace aeromatch
