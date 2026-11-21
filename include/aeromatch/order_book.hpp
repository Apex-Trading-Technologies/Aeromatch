#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <cstring>
#include "order.hpp"
#include "trade.hpp"
#include "enums.hpp"
#include "event_tap.hpp"
#include "infra/object_pool.hpp"

namespace aeromatch {

struct Array16Hash {
    std::size_t operator()(const std::array<unsigned char, 16>& a) const noexcept {
        uint64_t h1 = 0, h2 = 0;
        std::memcpy(&h1, a.data(), 8);
        std::memcpy(&h2, a.data() + 8, 8);
        return std::hash<uint64_t>{}(h1) ^ (std::hash<uint64_t>{}(h2) + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

struct AddOrderResult {
    OrderAcceptance acceptance;
    std::vector<Trade> trades;
    bool critical_error{false};
};

struct RejectionLog {
    uint64_t timestamp_ns;
    uint64_t order_id;
    int64_t price;
    int64_t quantity;
    Side side;
    OrderAcceptance reason;
};

struct IntrusiveOrderList {
    Order* head = nullptr;
    Order* tail = nullptr;

    bool empty() const noexcept { return head == nullptr; }

    void push_back(Order* order) noexcept {
        order->next = nullptr;
        order->prev = tail;
        if (tail) tail->next = order;
        else head = order;
        tail = order;
    }

    void erase(Order* order) noexcept {
        if (order->prev) order->prev->next = order->next;
        else head = order->next;
        if (order->next) order->next->prev = order->prev;
        else tail = order->prev;
        order->next = order->prev = nullptr;
    }
};

class OrderBook {
public:
    explicit OrderBook(const char* symbol = nullptr,
                       ObjectPool<Order>* pool = nullptr,
                       uint32_t max_active_orders = config::MAX_ACTIVE_ORDERS_PER_SYMBOL,
                       uint32_t max_orders_per_user = config::MAX_ACTIVE_ORDERS_PER_USER,
                       ITradeEventTap* tap = nullptr);

    void set_pool(ObjectPool<Order>* pool) noexcept { pool_ = pool; }
    void set_event_tap(ITradeEventTap* tap) noexcept { event_tap_ = tap; }

    AddOrderResult add_order(Order&& order);
    bool cancel_order(uint64_t order_id);

    int64_t best_bid() const;
    int64_t best_ask() const;
    bool has_order(uint64_t order_id) const;
    uint32_t active_order_count() const { return active_order_count_; }
    uint32_t user_active_order_count(const std::array<unsigned char, 16>& user_id) const {
        auto it = user_order_counts_.find(user_id);
        return (it != user_order_counts_.end()) ? it->second : 0;
    }
    bool critical_error() const { return critical_error_; }

    void reset() {
        bids_ = PriceLevelMap{};
        asks_ = AskPriceLevelMap{};
        bid_lookup_ = {};
        ask_lookup_ = {};
        user_order_counts_ = {};
        active_order_count_ = 0;
        critical_error_ = false;
        next_trade_id_ = 1;
    }

    using RejectionLogger = std::function<void(const RejectionLog&)>;
    void set_rejection_logger(RejectionLogger logger) { rejection_logger_ = logger; }

private:
    using Price = int64_t;
    using PriceLevelMap = std::map<Price, IntrusiveOrderList, std::greater<Price>>;
    using AskPriceLevelMap = std::map<Price, IntrusiveOrderList>;
    using LookupEntry = std::pair<PriceLevelMap::iterator, Order*>;
    using AskLookupEntry = std::pair<AskPriceLevelMap::iterator, Order*>;

    std::array<char, config::SYMBOL_LEN> symbol_;
    PriceLevelMap bids_;
    AskPriceLevelMap asks_;
    std::unordered_map<uint64_t, LookupEntry> bid_lookup_;
    std::unordered_map<uint64_t, AskLookupEntry> ask_lookup_;
    std::unordered_map<std::array<unsigned char, 16>, uint32_t, Array16Hash> user_order_counts_;

    uint64_t next_trade_id_{1};
    uint32_t active_order_count_{0};
    uint32_t max_active_orders_;
    uint32_t max_orders_per_user_;
    bool critical_error_{false};

    ObjectPool<Order>* pool_{nullptr};
    ITradeEventTap* event_tap_{nullptr};
    RejectionLogger rejection_logger_;

    std::vector<Trade> match_buy_order(Order& buy_order);
    std::vector<Trade> match_sell_order(Order& sell_order);
    bool can_fully_fill_buy(const Order& buy_order) const;
    bool can_fully_fill_sell(const Order& sell_order) const;
    void add_resting_order(Order&& order);
    bool validate_order(const Order& order, OrderAcceptance& reason) const;
    void log_rejection(const Order& order, OrderAcceptance reason);
    void return_order_to_pool(Order* order);
    static uint64_t now_ns();
};

} // namespace aeromatch
