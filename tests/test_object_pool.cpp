#include <gtest/gtest.h>
#include "aeromatch/infra/object_pool.hpp"

using namespace aeromatch;

struct Dummy {
    int x{0};
    double y{0.0};
};

TEST(ObjectPoolTest, BasicAcquireAndRelease) {
    ObjectPool<Dummy> pool(100);
    EXPECT_EQ(pool.used_count(), 0);

    Dummy* d1 = pool.acquire();
    ASSERT_NE(d1, nullptr);
    EXPECT_EQ(pool.used_count(), 1);

    pool.release(d1);
    EXPECT_EQ(pool.used_count(), 0);
}

TEST(ObjectPoolTest, ExhaustionAtCapacity) {
    ObjectPool<Dummy> pool(2, 1);
    Dummy* d1 = pool.acquire();
    Dummy* d2 = pool.acquire();
    ASSERT_NE(d1, nullptr);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(pool.used_count(), 2);

    Dummy* d3 = pool.acquire();
    EXPECT_EQ(d3, nullptr);

    pool.release(d1);
    Dummy* d4 = pool.acquire();
    ASSERT_NE(d4, nullptr);
}
