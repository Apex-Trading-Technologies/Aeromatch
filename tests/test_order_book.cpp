#include <gtest/gtest.h>
#include "aeromatch/order_book.hpp"
#include "aeromatch/infra/object_pool.hpp"
#include <memory>

using namespace aeromatch;

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = std::make_unique<ObjectPool<Order>>(100000);
        book = std::make_unique<OrderBook>("BTCUSDT", pool.get(), 10000);
        book->reset();
    }

    Order make_order(uint64_t id, Side side = Side::BUY, OrderType type = OrderType::LIMIT,
                     int64_t price = 0, uint64_t qty = 0) {
        return Order(id, "BTCUSDT", side, type, price, qty);
    }

    std::unique_ptr<ObjectPool<Order>> pool;
    std::unique_ptr<OrderBook> book;
};

TEST_F(OrderBookTest, SingleLimitOrderResting) {
    auto order = make_order(1, Side::BUY, OrderType::LIMIT, 100, 10);
    auto result = book->add_order(std::move(order));
    EXPECT_EQ(result.acceptance, OrderAcceptance::ACCEPTED);
    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book->active_order_count(), 1);
    EXPECT_EQ(book->best_bid(), 100);
}

TEST_F(OrderBookTest, DuplicateOrderIdRejection) {
    book->add_order(make_order(1, Side::BUY, OrderType::LIMIT, 100, 10));
    auto result = book->add_order(make_order(1, Side::BUY, OrderType::LIMIT, 100, 10));
    EXPECT_EQ(result.acceptance, OrderAcceptance::REJECTED_DUPLICATE_ID);
    EXPECT_EQ(book->active_order_count(), 1);
}

TEST_F(OrderBookTest, FullTradeExecution) {
    book->add_order(make_order(1, Side::SELL, OrderType::LIMIT, 100, 10));
    auto result = book->add_order(make_order(2, Side::BUY, OrderType::LIMIT, 100, 10));
    EXPECT_EQ(result.acceptance, OrderAcceptance::ACCEPTED);
    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].price, 100);
    EXPECT_EQ(result.trades[0].quantity, 10);
    EXPECT_EQ(book->active_order_count(), 0);
}

TEST_F(OrderBookTest, PartialFill) {
    book->add_order(make_order(1, Side::SELL, OrderType::LIMIT, 100, 5));
    book->add_order(make_order(2, Side::SELL, OrderType::LIMIT, 101, 5));
    auto result = book->add_order(make_order(3, Side::BUY, OrderType::LIMIT, 101, 10));
    EXPECT_EQ(result.acceptance, OrderAcceptance::ACCEPTED);
    EXPECT_EQ(result.trades.size(), 2);
    EXPECT_EQ(result.trades[0].price, 100);
    EXPECT_EQ(result.trades[1].price, 101);
    EXPECT_EQ(book->active_order_count(), 0);
}

TEST_F(OrderBookTest, PriceTimePriority) {
    book->add_order(make_order(1, Side::SELL, OrderType::LIMIT, 100, 5));
    book->add_order(make_order(2, Side::SELL, OrderType::LIMIT, 100, 5));
    auto result = book->add_order(make_order(3, Side::BUY, OrderType::LIMIT, 100, 5));
    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].sell_order_id, 1);
}

TEST_F(OrderBookTest, CancelOrder) {
    book->add_order(make_order(1, Side::BUY, OrderType::LIMIT, 100, 10));
    EXPECT_TRUE(book->cancel_order(1));
    EXPECT_EQ(book->active_order_count(), 0);
    EXPECT_FALSE(book->cancel_order(1));
}

TEST_F(OrderBookTest, IOCUnfilledCancellation) {
    book->add_order(make_order(1, Side::SELL, OrderType::LIMIT, 105, 10));
    auto result = book->add_order(make_order(2, Side::BUY, OrderType::IOC, 100, 10));
    EXPECT_EQ(result.acceptance, OrderAcceptance::ACCEPTED_ZERO_FILL);
    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book->active_order_count(), 1);
}

TEST_F(OrderBookTest, FOKUnfillableRejection) {
    book->add_order(make_order(1, Side::SELL, OrderType::LIMIT, 100, 5));
    auto result = book->add_order(make_order(2, Side::BUY, OrderType::FOK, 100, 10));
    EXPECT_EQ(result.acceptance, OrderAcceptance::REJECTED_FOK_UNFILLABLE);
    EXPECT_EQ(book->active_order_count(), 1);
}

TEST_F(OrderBookTest, ArithmeticOverflowNotionalRejection) {
    auto order = make_order(101, Side::BUY, OrderType::LIMIT, 3'000'000'000LL, 4'000'000'000LL);
    auto result = book->add_order(std::move(order));
    EXPECT_EQ(result.acceptance, OrderAcceptance::REJECTED_INVALID_INPUT);
    EXPECT_FALSE(book->has_order(101));
}

TEST_F(OrderBookTest, PerUserMaxActiveOrdersLimit) {
    OrderBook limited_book("BTCUSDT", pool.get(), 1000, 2);
    unsigned char test_uid[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    Order o1(1, test_uid, "BTCUSDT", Side::BUY, OrderType::LIMIT, 100, 10);
    Order o2(2, test_uid, "BTCUSDT", Side::BUY, OrderType::LIMIT, 99, 10);
    Order o3(3, test_uid, "BTCUSDT", Side::BUY, OrderType::LIMIT, 98, 10);

    EXPECT_EQ(limited_book.add_order(std::move(o1)).acceptance, OrderAcceptance::ACCEPTED);
    EXPECT_EQ(limited_book.add_order(std::move(o2)).acceptance, OrderAcceptance::ACCEPTED);
    EXPECT_EQ(limited_book.add_order(std::move(o3)).acceptance, OrderAcceptance::REJECTED_RATE_LIMITED);

    EXPECT_TRUE(limited_book.cancel_order(1));
    Order o4(4, test_uid, "BTCUSDT", Side::BUY, OrderType::LIMIT, 97, 10);
    EXPECT_EQ(limited_book.add_order(std::move(o4)).acceptance, OrderAcceptance::ACCEPTED);
}

TEST_F(OrderBookTest, EventTapNotified) {
    class MockTap : public ITradeEventTap {
    public:
        int trades_count{0};
        int accepted_count{0};
        int rejected_count{0};
        void on_trade(const Trade&) override { ++trades_count; }
        void on_order_accepted(const Order&) override { ++accepted_count; }
        void on_order_rejected(uint64_t, uint8_t) override { ++rejected_count; }
    } tap;

    OrderBook tap_book("BTCUSDT", pool.get(), 1000, 100, &tap);

    tap_book.add_order(make_order(1, Side::SELL, OrderType::LIMIT, 100, 10));
    EXPECT_EQ(tap.accepted_count, 1);
    EXPECT_EQ(tap.trades_count, 0);

    tap_book.add_order(make_order(2, Side::BUY, OrderType::LIMIT, 100, 10));
    EXPECT_EQ(tap.accepted_count, 2);
    EXPECT_EQ(tap.trades_count, 1);

    // Invalid order triggers rejection
    tap_book.add_order(make_order(3, Side::BUY, OrderType::LIMIT, -10, 10));
    EXPECT_EQ(tap.rejected_count, 1);
}
