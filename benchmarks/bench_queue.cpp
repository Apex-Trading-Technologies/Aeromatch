#include <benchmark/benchmark.h>
#include "aeromatch/infra/ring_buffer.hpp"
#include <thread>
#include <atomic>

using namespace aeromatch;

static void BM_RingBuffer_SPSC_Throughput(benchmark::State& st) {
    const size_t CAPACITY = 1024;
    RingBuffer<int, CAPACITY> rb;
    int val = 42;

    for (auto _ : st) {
        while (rb.try_push(42)) {}
        while (rb.try_pop(val)) {}
        benchmark::DoNotOptimize(val);
    }
    st.SetItemsProcessed(st.iterations() * CAPACITY * 2);
}
BENCHMARK(BM_RingBuffer_SPSC_Throughput)->Unit(benchmark::kMillisecond);

static void BM_RingBuffer_SPSC_MT(benchmark::State& st) {
    const size_t CAPACITY = 2048;
    RingBuffer<int, CAPACITY> rb;
    std::atomic<bool> stop{false};

    std::thread consumer([&]() {
        int val;
        while (!stop.load(std::memory_order_acquire)) {
            while (rb.try_pop(val)) {
                benchmark::DoNotOptimize(val);
            }
        }
    });

    for (auto _ : st) {
        for (int i = 0; i < 100'000; ++i) {
            int item = i;
            while (!rb.try_push(std::move(item))) {
                std::this_thread::yield();
            }
        }
    }

    stop.store(true, std::memory_order_release);
    consumer.join();
    st.SetItemsProcessed(st.iterations() * 100'000);
}
BENCHMARK(BM_RingBuffer_SPSC_MT)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
