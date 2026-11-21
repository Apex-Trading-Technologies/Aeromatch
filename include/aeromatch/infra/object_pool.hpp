#pragma once

#include <vector>
#include <memory>
#include <cstddef>
#include <new>
#include <type_traits>
#include <algorithm>

namespace aeromatch {

/**
 * Chunked object pool with a fixed maximum capacity.
 * Starts small and grows lazily in fixed-size chunks up to `max_capacity`.
 * Designed for single-threaded zero-allocation inner matching loops.
 */
template <typename T>
class ObjectPool {
public:
    static constexpr size_t kDefaultChunkSize = 25'000;

    explicit ObjectPool(size_t max_capacity,
                        size_t chunk_size = kDefaultChunkSize)
        : max_capacity_(max_capacity)
        , chunk_size_(chunk_size == 0 ? kDefaultChunkSize : chunk_size)
        , used_(0) {
        grow();
    }

    ~ObjectPool() = default;

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    T* acquire() noexcept {
        if (free_list_.empty()) {
            if (!grow()) {
                return nullptr; // reached max capacity
            }
        }
        T* ptr = free_list_.back();
        free_list_.pop_back();
        ++used_;
        return ptr;
    }

    void release(T* ptr) noexcept {
        if (!ptr) return;
        ptr->~T();
        free_list_.push_back(ptr);
        --used_;
    }

    void reset() noexcept {
        free_list_.clear();
        for (auto& chunk : chunks_) {
            for (size_t i = 0; i < chunk.size; ++i) {
                free_list_.push_back(&chunk.data[i]);
            }
        }
        used_ = 0;
    }

    size_t capacity()        const noexcept { return max_capacity_; }
    size_t used_count()      const noexcept { return used_; }
    size_t free_count()      const noexcept { return free_list_.size(); }
    size_t allocated_count() const noexcept {
        size_t total = 0;
        for (const auto& c : chunks_) total += c.size;
        return total;
    }

private:
    struct Chunk {
        std::unique_ptr<T[]> data;
        size_t size = 0;
    };

    bool grow() noexcept {
        size_t already = 0;
        for (const auto& c : chunks_) already += c.size;
        if (already >= max_capacity_) return false;

        size_t to_alloc = std::min(chunk_size_, max_capacity_ - already);

        Chunk c;
        try {
            c.data = std::make_unique<T[]>(to_alloc);
        } catch (...) {
            return false;
        }
        c.size = to_alloc;

        free_list_.reserve(free_list_.size() + to_alloc);
        for (size_t i = 0; i < to_alloc; ++i) {
            free_list_.push_back(&c.data[i]);
        }

        chunks_.push_back(std::move(c));
        return true;
    }

    std::vector<Chunk>  chunks_;
    std::vector<T*>     free_list_;
    size_t              max_capacity_;
    size_t              chunk_size_;
    size_t              used_;
};

} // namespace aeromatch
