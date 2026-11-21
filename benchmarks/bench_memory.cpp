#include <benchmark/benchmark.h>
#include "aeromatch/infra/object_pool.hpp"
#include <vector>
#include <memory>

using namespace aeromatch;

struct BenchTrade {
    uint64_t id;
    uint32_t price;
    uint32_t qty;
    uint64_t timestamp;
    char pad[32];
};

static void BM_NewDelete(benchmark::State& st) {
    for (auto _ : st) {
        BenchTrade* t = new BenchTrade();
        benchmark::DoNotOptimize(t);
        delete t;
    }
    st.SetItemsProcessed(st.iterations());
}
BENCHMARK(BM_NewDelete);

static void BM_ObjectPool(benchmark::State& st) {
    ObjectPool<BenchTrade> pool(1024);
    for (auto _ : st) {
        BenchTrade* t = pool.acquire();
        if (t) {
            new (t) BenchTrade();
            benchmark::DoNotOptimize(t);
            pool.release(t);
        }
    }
    st.SetItemsProcessed(st.iterations());
}
BENCHMARK(BM_ObjectPool);

static void BM_ObjectPool_Batch(benchmark::State& st) {
    ObjectPool<BenchTrade> pool(4096);
    std::vector<BenchTrade*> handles;
    handles.reserve(256);

    for (auto _ : st) {
        for (int i = 0; i < 256; ++i) {
            BenchTrade* t = pool.acquire();
            if (t) {
                new (t) BenchTrade();
                handles.push_back(t);
            }
        }
        for (BenchTrade* t : handles) {
            pool.release(t);
        }
        handles.clear();
    }
    st.SetItemsProcessed(st.iterations() * 256);
}
BENCHMARK(BM_ObjectPool_Batch);

BENCHMARK_MAIN();
