#include <gtest/gtest.h>
#include "aeromatch/matching_engine.hpp"

using namespace aeromatch;

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = std::make_unique<ObjectPool<Order>>(100000);
        engine = std::make_unique<MatchingEngine>(metrics, *pool);
    }

    Metrics metrics;
    std::unique_ptr<ObjectPool<Order>> pool;
    std::unique_ptr<MatchingEngine> engine;
};

TEST_F(MatchingEngineTest, SynchronousProcessOrder) {
    Order sell_order(1, "BTCUSDT", Side::SELL, OrderType::LIMIT, 50000, 10);
    auto res1 = engine->process_order(std::move(sell_order));
    EXPECT_EQ(res1.acceptance, OrderAcceptance::ACCEPTED);
    EXPECT_TRUE(res1.trades.empty());

    Order buy_order(2, "BTCUSDT", Side::BUY, OrderType::LIMIT, 50000, 10);
    auto res2 = engine->process_order(std::move(buy_order));
    EXPECT_EQ(res2.acceptance, OrderAcceptance::ACCEPTED);
    EXPECT_EQ(res2.trades.size(), 1);
    EXPECT_EQ(res2.trades[0].price, 50000);
    EXPECT_EQ(res2.trades[0].quantity, 10);

    auto snap = metrics.snapshot();
    EXPECT_EQ(snap.orders_received, 2);
    EXPECT_EQ(snap.orders_matched, 1);
    EXPECT_EQ(snap.trades_executed, 1);
}

TEST_F(MatchingEngineTest, UnknownSymbolRejected) {
    Order unknown_order(1, "UNKNOWNXYZ", Side::BUY, OrderType::LIMIT, 100, 10);
    auto res = engine->process_order(std::move(unknown_order));
    EXPECT_EQ(res.acceptance, OrderAcceptance::REJECTED_INVALID_INPUT);

    auto snap = metrics.snapshot();
    EXPECT_EQ(snap.orders_rejected, 1);
}

TEST_F(MatchingEngineTest, AsyncRingBufferExecution) {
    engine->start();

    auto& rb = engine->get_ring_buffer();
    EXPECT_TRUE(rb.try_push(Order(10, "ETHUSDT", Side::SELL, OrderType::LIMIT, 3000, 5)));
    EXPECT_TRUE(rb.try_push(Order(11, "ETHUSDT", Side::BUY, OrderType::LIMIT, 3000, 5)));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto snap = metrics.snapshot();
    EXPECT_GE(snap.orders_received, 2);
    EXPECT_GE(snap.trades_executed, 1);

    engine->stop();
}

TEST_F(MatchingEngineTest, CustomSymbolRegistration) {
    SymbolInfo custom;
    custom.symbol = "SOLUSDC";
    custom.asset_class = AssetClass::CRYPTO;
    custom.currency = Currency::USDC;
    custom.tick_size = 1;
    custom.lot_size = 1;
    custom.min_price = 1;
    custom.max_price = 1000000;

    engine->register_symbol(custom);
    EXPECT_TRUE(engine->has_symbol("SOLUSDC"));

    Order o(1, "SOLUSDC", Side::BUY, OrderType::LIMIT, 200, 5);
    auto res = engine->process_order(std::move(o));
    EXPECT_EQ(res.acceptance, OrderAcceptance::ACCEPTED);
}
