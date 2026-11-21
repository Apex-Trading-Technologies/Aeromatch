#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>

namespace aeromatch {

/**
 * High-performance lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer.
 * Uses power-of-two capacity for fast bitwise modulo wrapping.
 */
template <typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    RingBuffer() noexcept = default;

    bool try_push(T&& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & (Capacity - 1);
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[head] = std::move(item);
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        out = std::move(buffer_[tail]);
        tail_.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }

    size_t size_approx() const noexcept {
        auto head = head_.load(std::memory_order_acquire);
        auto tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & (Capacity - 1);
    }

    static constexpr size_t capacity() noexcept { return Capacity; }

private:
    T buffer_[Capacity];
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

} // namespace aeromatch
