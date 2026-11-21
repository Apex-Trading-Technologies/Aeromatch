#include <gtest/gtest.h>
#include "aeromatch/infra/ring_buffer.hpp"
#include <thread>
#include <vector>
#include <numeric>

using namespace aeromatch;

TEST(RingBufferTest, SingleThreadedFIFO) {
    RingBuffer<int, 8> rb;
    EXPECT_EQ(rb.capacity(), 8);

    EXPECT_TRUE(rb.try_push(10));
    EXPECT_TRUE(rb.try_push(20));

    int val = 0;
    EXPECT_TRUE(rb.try_pop(val));
    EXPECT_EQ(val, 10);
    EXPECT_TRUE(rb.try_pop(val));
    EXPECT_EQ(val, 20);
    EXPECT_FALSE(rb.try_pop(val));
}

TEST(RingBufferTest, SPSCCurrency) {
    RingBuffer<int, 1024> rb;
    const int count = 50000;
    std::atomic<bool> done{false};

    std::thread producer([&]() {
        for (int i = 0; i < count; ++i) {
            while (!rb.try_push(int(i))) {
                std::this_thread::yield();
            }
        }
        done.store(true);
    });

    std::vector<int> received;
    received.reserve(count);

    std::thread consumer([&]() {
        int val = 0;
        while (!done.load() || received.size() < count) {
            if (rb.try_pop(val)) {
                received.push_back(val);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(received.size(), count);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(received[i], i);
    }
}
