#include <benchmark/benchmark.h>
#include "aeromatch/order_book.hpp"
#include "aeromatch/order.hpp"
#include "aeromatch/infra/object_pool.hpp"

using namespace aeromatch;

inline Order make_order(uint64_t id, Side side, OrderType type, int64_t price, int64_t qty) {
    return Order(id, "BTCUSDT", side, type, price, qty);
}

static void BM_OrderBook_Throughput(benchmark::State& state) {
    const int BATCH_SIZE = 1000;
    ObjectPool<Order> pool(1'000'000);
    OrderBook book("BTCUSDT", &pool, 1'000'000);

    uint64_t id = 1;
    for (auto _ : state) {
        for (int i = 0; i < BATCH_SIZE; ++i) {
            Order order = make_order(id++, Side::BUY, OrderType::LIMIT, 100, 10);
            auto result = book.add_order(std::move(order));
            benchmark::DoNotOptimize(result);
        }
        benchmark::DoNotOptimize(book);
    }
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}
BENCHMARK(BM_OrderBook_Throughput);

static void BM_OrderBook_Latency(benchmark::State& state) {
    ObjectPool<Order> pool(1'000'000);
    OrderBook book("BTCUSDT", &pool, 1'000'000);
    uint64_t id = 1;

    for (auto _ : state) {
        Order order = make_order(id++, Side::BUY, OrderType::LIMIT, 100, 10);
        auto result = book.add_order(std::move(order));
        benchmark::DoNotOptimize(result);
    }
    benchmark::DoNotOptimize(book);
}
BENCHMARK(BM_OrderBook_Latency);

static void BM_OrderBook_Crossing(benchmark::State& state) {
    const int BATCH_SIZE = 1000;
    ObjectPool<Order> pool(1'000'000);
    OrderBook book("BTCUSDT", &pool, 1'000'000);

    uint64_t id = 1;
    for (auto _ : state) {
        for (int i = 0; i < BATCH_SIZE / 2; ++i) {
            int64_t price = 95 + (i % 11);
            Order order = make_order(id++, Side::SELL, OrderType::LIMIT, price, 10);
            book.add_order(std::move(order));
        }
        for (int i = 0; i < BATCH_SIZE / 2; ++i) {
            Order order = make_order(id++, Side::BUY, OrderType::LIMIT, 100, 10);
            auto result = book.add_order(std::move(order));
            benchmark::DoNotOptimize(result);
        }
        benchmark::DoNotOptimize(book);
    }
    state.SetItemsProcessed(state.iterations() * BATCH_SIZE);
}
BENCHMARK(BM_OrderBook_Crossing);

static void BM_OrderBook_CrossingLatency(benchmark::State& state) {
    ObjectPool<Order> pool(1'000'000);
    OrderBook book("BTCUSDT", &pool, 1'000'000);
    uint64_t id = 1;

    Order sell = make_order(id++, Side::SELL, OrderType::LIMIT, 100, 10);
    book.add_order(std::move(sell));

    for (auto _ : state) {
        Order buy = make_order(id++, Side::BUY, OrderType::LIMIT, 100, 10);
        auto result = book.add_order(std::move(buy));
        benchmark::DoNotOptimize(result);
        Order new_sell = make_order(id++, Side::SELL, OrderType::LIMIT, 100, 10);
        book.add_order(std::move(new_sell));
    }
    benchmark::DoNotOptimize(book);
}
BENCHMARK(BM_OrderBook_CrossingLatency);

BENCHMARK_MAIN();
