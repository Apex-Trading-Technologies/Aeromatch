#include "aeromatch/order_book.hpp"
#include <cstring>
#include <chrono>
#include <algorithm>

namespace aeromatch {

OrderBook::OrderBook(const char* symbol,
                     ObjectPool<Order>* pool,
                     uint32_t max_active_orders,
                     uint32_t max_orders_per_user,
                     ITradeEventTap* tap)
    : max_active_orders_(max_active_orders)
    , max_orders_per_user_(max_orders_per_user)
    , pool_(pool)
    , event_tap_(tap) {
    reset();

    if (symbol) {
        std::strncpy(symbol_.data(), symbol, config::SYMBOL_LEN - 1);
        symbol_[config::SYMBOL_LEN - 1] = '\0';
    } else {
        symbol_.fill('\0');
    }
}

uint64_t OrderBook::now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

void OrderBook::return_order_to_pool(Order* order) {
    if (pool_ && order) {
        order->~Order();
        pool_->release(order);
    }
}

bool OrderBook::validate_order(const Order& order, OrderAcceptance& reason) const {
    if (order.quantity <= 0) {
        reason = OrderAcceptance::REJECTED_INVALID_INPUT;
        return false;
    }
    if (order.type != OrderType::MARKET && order.price <= 0) {
        reason = OrderAcceptance::REJECTED_INVALID_INPUT;
        return false;
    }
    if (order.side != Side::BUY && order.side != Side::SELL) {
        reason = OrderAcceptance::REJECTED_INVALID_INPUT;
        return false;
    }
    if (static_cast<uint8_t>(order.type) > static_cast<uint8_t>(OrderType::FOK)) {
        reason = OrderAcceptance::REJECTED_INVALID_INPUT;
        return false;
    }

    // Arithmetic overflow protection on price * quantity using __uint128_t
    if (order.price > 0 && order.quantity > 0) {
        __uint128_t notional = static_cast<__uint128_t>(order.price) * static_cast<__uint128_t>(order.quantity);
        if (notional > static_cast<__uint128_t>(std::numeric_limits<int64_t>::max())) {
            reason = OrderAcceptance::REJECTED_INVALID_INPUT;
            return false;
        }
    }

    if (has_order(order.order_id)) {
        reason = OrderAcceptance::REJECTED_DUPLICATE_ID;
        return false;
    }
    return true;
}

void OrderBook::log_rejection(const Order& order, OrderAcceptance reason) {
    if (rejection_logger_) {
        RejectionLog log{};
        log.timestamp_ns = now_ns();
        log.order_id = order.order_id;
        log.price = order.price;
        log.quantity = order.quantity;
        log.side = order.side;
        log.reason = reason;
        rejection_logger_(log);
    }
    if (event_tap_) {
        event_tap_->on_order_rejected(order.order_id, static_cast<uint8_t>(reason));
    }
}

bool OrderBook::has_order(uint64_t order_id) const {
    return bid_lookup_.find(order_id) != bid_lookup_.end() ||
           ask_lookup_.find(order_id) != ask_lookup_.end();
}

int64_t OrderBook::best_bid() const {
    if (bids_.empty()) return 0;
    return bids_.begin()->first;
}

int64_t OrderBook::best_ask() const {
    if (asks_.empty()) return 0;
    return asks_.begin()->first;
}

bool OrderBook::can_fully_fill_buy(const Order& buy_order) const {
    int64_t needed = buy_order.quantity;
    for (const auto& [price, list] : asks_) {
        if (price > buy_order.price) break;
        for (Order* curr = list.head; curr != nullptr; curr = curr->next) {
            needed -= curr->remaining_quantity;
            if (needed <= 0) return true;
        }
    }
    return false;
}

bool OrderBook::can_fully_fill_sell(const Order& sell_order) const {
    int64_t needed = sell_order.quantity;
    for (const auto& [price, list] : bids_) {
        if (price < sell_order.price) break;
        for (Order* curr = list.head; curr != nullptr; curr = curr->next) {
            needed -= curr->remaining_quantity;
            if (needed <= 0) return true;
        }
    }
    return false;
}

std::vector<Trade> OrderBook::match_buy_order(Order& buy_order) {
    std::vector<Trade> trades;
    auto ask_it = asks_.begin();

    while (ask_it != asks_.end() && buy_order.remaining_quantity > 0) {
        if (ask_it->first > buy_order.price && buy_order.type != OrderType::MARKET) {
            break;
        }

        auto& list = ask_it->second;
        Order* curr = list.head;

        while (curr != nullptr && buy_order.remaining_quantity > 0) {
            Order* next_order = curr->next;
            int64_t fill_qty = std::min(buy_order.remaining_quantity, curr->remaining_quantity);

            Trade trade{};
            trade.trade_id = next_trade_id_++;
            trade.buy_order_id = buy_order.order_id;
            trade.sell_order_id = curr->order_id;
            trade.price = curr->price;
            trade.quantity = fill_qty;
            trade.timestamp_ns = now_ns();
            trade.symbol = std::string(symbol_.data());
            trade.side = "buy";
            trade.user_id = buy_order.user_id;

            trades.push_back(trade);
            if (event_tap_) {
                event_tap_->on_trade(trade);
            }

            buy_order.remaining_quantity -= fill_qty;
            curr->remaining_quantity -= fill_qty;

            if (curr->remaining_quantity == 0) {
                list.erase(curr);
                ask_lookup_.erase(curr->order_id);
                active_order_count_--;
                auto uit = user_order_counts_.find(curr->user_id);
                if (uit != user_order_counts_.end()) {
                    if (--uit->second == 0) {
                        user_order_counts_.erase(uit);
                    }
                }
                return_order_to_pool(curr);
            }

            curr = next_order;
        }

        if (list.empty()) {
            ask_it = asks_.erase(ask_it);
        } else {
            ++ask_it;
        }
    }

    return trades;
}

std::vector<Trade> OrderBook::match_sell_order(Order& sell_order) {
    std::vector<Trade> trades;
    auto bid_it = bids_.begin();

    while (bid_it != bids_.end() && sell_order.remaining_quantity > 0) {
        if (bid_it->first < sell_order.price && sell_order.type != OrderType::MARKET) {
            break;
        }

        auto& list = bid_it->second;
        Order* curr = list.head;

        while (curr != nullptr && sell_order.remaining_quantity > 0) {
            Order* next_order = curr->next;
            int64_t fill_qty = std::min(sell_order.remaining_quantity, curr->remaining_quantity);

            Trade trade{};
            trade.trade_id = next_trade_id_++;
            trade.buy_order_id = curr->order_id;
            trade.sell_order_id = sell_order.order_id;
            trade.price = curr->price;
            trade.quantity = fill_qty;
            trade.timestamp_ns = now_ns();
            trade.symbol = std::string(symbol_.data());
            trade.side = "sell";
            trade.user_id = sell_order.user_id;

            trades.push_back(trade);
            if (event_tap_) {
                event_tap_->on_trade(trade);
            }

            sell_order.remaining_quantity -= fill_qty;
            curr->remaining_quantity -= fill_qty;

            if (curr->remaining_quantity == 0) {
                list.erase(curr);
                bid_lookup_.erase(curr->order_id);
                active_order_count_--;
                auto uit = user_order_counts_.find(curr->user_id);
                if (uit != user_order_counts_.end()) {
                    if (--uit->second == 0) {
                        user_order_counts_.erase(uit);
                    }
                }
                return_order_to_pool(curr);
            }

            curr = next_order;
        }

        if (list.empty()) {
            bid_it = bids_.erase(bid_it);
        } else {
            ++bid_it;
        }
    }

    return trades;
}

void OrderBook::add_resting_order(Order&& order) {
    Order* ptr = pool_ ? pool_->acquire() : new Order();
    if (!ptr) {
        critical_error_ = true;
        return;
    }

    new (ptr) Order(std::move(order));

    if (ptr->side == Side::BUY) {
        auto& list = bids_[ptr->price];
        list.push_back(ptr);
        bid_lookup_[ptr->order_id] = {bids_.find(ptr->price), ptr};
    } else {
        auto& list = asks_[ptr->price];
        list.push_back(ptr);
        ask_lookup_[ptr->order_id] = {asks_.find(ptr->price), ptr};
    }
    active_order_count_++;

    bool is_nil_user = true;
    for (auto b : ptr->user_id) {
        if (b != 0) { is_nil_user = false; break; }
    }
    if (!is_nil_user) {
        user_order_counts_[ptr->user_id]++;
    }
}

AddOrderResult OrderBook::add_order(Order&& order) {
    OrderAcceptance reason;
    if (!validate_order(order, reason)) {
        log_rejection(order, reason);
        return AddOrderResult{reason, {}, false};
    }

    if (order.type == OrderType::FOK) {
        bool fillable = (order.side == Side::BUY) ? can_fully_fill_buy(order) : can_fully_fill_sell(order);
        if (!fillable) {
            log_rejection(order, OrderAcceptance::REJECTED_FOK_UNFILLABLE);
            return AddOrderResult{OrderAcceptance::REJECTED_FOK_UNFILLABLE, {}, false};
        }
    }

    std::vector<Trade> trades;
    if (order.side == Side::BUY) {
        trades = match_buy_order(order);
    } else {
        trades = match_sell_order(order);
    }

    // Limit orders with remaining quantity rest on the book
    if (order.type == OrderType::LIMIT && order.remaining_quantity > 0) {
        // Anti-DoS capacity check per symbol and per user
        if (active_order_count_ >= max_active_orders_) {
            log_rejection(order, OrderAcceptance::REJECTED_RATE_LIMITED);
            return AddOrderResult{OrderAcceptance::REJECTED_RATE_LIMITED, trades, false};
        }

        bool is_nil_user = true;
        for (auto b : order.user_id) {
            if (b != 0) { is_nil_user = false; break; }
        }
        if (!is_nil_user) {
            auto uit = user_order_counts_.find(order.user_id);
            if (uit != user_order_counts_.end() && uit->second >= max_orders_per_user_) {
                log_rejection(order, OrderAcceptance::REJECTED_RATE_LIMITED);
                return AddOrderResult{OrderAcceptance::REJECTED_RATE_LIMITED, trades, false};
            }
        }

        add_resting_order(std::move(order));
        if (critical_error_) {
            log_rejection(order, OrderAcceptance::REJECTED_NULL_POINTER);
            return AddOrderResult{OrderAcceptance::REJECTED_NULL_POINTER, trades, true};
        }
    }

    OrderAcceptance final_status;
    if (!trades.empty()) {
        final_status = OrderAcceptance::ACCEPTED;
    } else if (order.type == OrderType::LIMIT) {
        final_status = OrderAcceptance::ACCEPTED;
    } else {
        final_status = OrderAcceptance::ACCEPTED_ZERO_FILL;
    }

    if (event_tap_) {
        event_tap_->on_order_accepted(order);
    }

    return AddOrderResult{final_status, trades, false};
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto bit = bid_lookup_.find(order_id);
    if (bit != bid_lookup_.end()) {
        auto price_it = bit->second.first;
        Order* ptr = bit->second.second;
        price_it->second.erase(ptr);
        if (price_it->second.empty()) {
            bids_.erase(price_it);
        }
        auto user_id = ptr->user_id;
        bid_lookup_.erase(bit);
        active_order_count_--;
        auto uit = user_order_counts_.find(user_id);
        if (uit != user_order_counts_.end()) {
            if (--uit->second == 0) {
                user_order_counts_.erase(uit);
            }
        }
        return_order_to_pool(ptr);
        return true;
    }

    auto ait = ask_lookup_.find(order_id);
    if (ait != ask_lookup_.end()) {
        auto price_it = ait->second.first;
        Order* ptr = ait->second.second;
        price_it->second.erase(ptr);
        if (price_it->second.empty()) {
            asks_.erase(price_it);
        }
        auto user_id = ptr->user_id;
        ask_lookup_.erase(ait);
        active_order_count_--;
        auto uit = user_order_counts_.find(user_id);
        if (uit != user_order_counts_.end()) {
            if (--uit->second == 0) {
                user_order_counts_.erase(uit);
            }
        }
        return_order_to_pool(ptr);
        return true;
    }

    return false;
}

} // namespace aeromatch
