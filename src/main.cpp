#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include "aeromatch/matching_engine.hpp"

namespace {
std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int) {
    g_shutdown_requested.store(true);
}

class ConsoleEventTap : public aeromatch::ITradeEventTap {
public:
    void on_trade(const aeromatch::Trade& trade) override {
        spdlog::info("[TRADE] ID={} Symbol={} Price={} Qty={} BuyOrder={} SellOrder={}",
                     trade.trade_id, trade.symbol, trade.price, trade.quantity,
                     trade.buy_order_id, trade.sell_order_id);
    }

    void on_order_accepted(const aeromatch::Order& order) override {
        spdlog::debug("[ORDER ACCEPTED] ID={} Side={} Price={} Qty={}",
                      order.order_id, (order.side == aeromatch::Side::BUY ? "BUY" : "SELL"),
                      order.price, order.quantity);
    }

    void on_order_rejected(uint64_t order_id, uint8_t reason) override {
        spdlog::warn("[ORDER REJECTED] ID={} ReasonCode={}", order_id, reason);
    }
};
} // namespace

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=============================================================");
    spdlog::info(" AeroMatch High-Performance C++20 Limit Order Book Engine   ");
    spdlog::info(" Open-Source Edition | github.com/Apex-Trading-Technologies  ");
    spdlog::info("=============================================================");

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    aeromatch::Metrics metrics;
    aeromatch::ObjectPool<aeromatch::Order> order_pool(1'000'000);
    ConsoleEventTap event_tap;

    aeromatch::MatchingEngine engine(metrics, order_pool, &event_tap);
    engine.start();

    spdlog::info("Engine started. Submitting baseline test liquidity...");

    // Seed test book for BTCUSDT
    engine.process_order(aeromatch::Order(1, "BTCUSDT", aeromatch::Side::SELL, aeromatch::OrderType::LIMIT, 95000, 10));
    engine.process_order(aeromatch::Order(2, "BTCUSDT", aeromatch::Side::SELL, aeromatch::OrderType::LIMIT, 95100, 15));
    engine.process_order(aeromatch::Order(3, "BTCUSDT", aeromatch::Side::BUY, aeromatch::OrderType::LIMIT, 94900, 20));

    // Execute matching trade
    spdlog::info("Submitting crossing BUY order to trigger match...");
    engine.process_order(aeromatch::Order(4, "BTCUSDT", aeromatch::Side::BUY, aeromatch::OrderType::LIMIT, 95000, 5));

    const auto* book = engine.get_book("BTCUSDT");
    if (book) {
        spdlog::info("BTCUSDT Best Bid: {}, Best Ask: {}, Active Orders: {}",
                     book->best_bid(), book->best_ask(), book->active_order_count());
    }

    auto snapshot = metrics.snapshot();
    spdlog::info("Metrics -> Received: {}, Matched: {}, Trades: {}",
                 snapshot.orders_received, snapshot.orders_matched, snapshot.trades_executed);

    spdlog::info("AeroMatch is running cleanly. Press Ctrl+C to terminate.");
    while (!g_shutdown_requested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (argc > 1 && std::string(argv[1]) == "--test-run") {
            break; // Clean exit for CI/smoke tests
        }
    }

    spdlog::info("Shutting down AeroMatch engine...");
    engine.stop();
    spdlog::info("Shutdown complete.");
    return 0;
}
