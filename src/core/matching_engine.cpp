#include "aeromatch/matching_engine.hpp"
#include <chrono>
#include <utility>
#include <spdlog/spdlog.h>

namespace aeromatch {

MatchingEngine::MatchingEngine(Metrics& metrics,
                               ObjectPool<Order>& order_pool,
                               ITradeEventTap* event_tap,
                               uint32_t max_symbols,
                               uint32_t max_active_orders_per_symbol,
                               uint32_t max_active_orders_per_user)
    : metrics_(metrics)
    , order_pool_(order_pool)
    , event_tap_(event_tap ? event_tap : &default_tap_)
    , max_symbols_(max_symbols)
    , max_active_orders_per_symbol_(max_active_orders_per_symbol)
    , max_active_orders_per_user_(max_active_orders_per_user) {
    books_.reserve(max_symbols_);
    register_default_symbols();
    spdlog::info("AeroMatch Engine initialized (standalone OSS, max_symbols={})", max_symbols_);
}

MatchingEngine::~MatchingEngine() {
    if (running_.load(std::memory_order_acquire)) {
        stop();
    }
}

void MatchingEngine::register_default_symbols() {
    std::lock_guard<std::mutex> lock(symbols_mutex_);
    for (const char* sym : {"BTCUSDT", "ETHUSDT", "SOLUSDT", "AAPL", "MSFT", "GOOG", "AMZN", "NVDA", "TEST"}) {
        SymbolInfo info;
        info.symbol = sym;
        info.asset_class = (std::string_view(sym).find("USDT") != std::string_view::npos) ? AssetClass::CRYPTO : AssetClass::EQUITY;
        info.currency = Currency::USD;
        info.tick_size = 1;
        info.lot_size = 1;
        info.min_price = 1;
        info.max_price = 100'000'000'000LL;
        symbols_[sym] = info;
    }
}

void MatchingEngine::register_symbol(const SymbolInfo& info) {
    std::lock_guard<std::mutex> lock(symbols_mutex_);
    symbols_[info.symbol] = info;
}

bool MatchingEngine::has_symbol(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(symbols_mutex_);
    return symbols_.find(symbol) != symbols_.end();
}

const SymbolInfo* MatchingEngine::get_symbol(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(symbols_mutex_);
    auto it = symbols_.find(symbol);
    return (it != symbols_.end()) ? &it->second : nullptr;
}

void MatchingEngine::start() {
    if (running_.load(std::memory_order_acquire)) {
        return;
    }
    running_.store(true, std::memory_order_release);
    engine_thread_ = std::thread(&MatchingEngine::run, this);
    spdlog::info("AeroMatch worker thread started");
}

void MatchingEngine::stop() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }
    running_.store(false, std::memory_order_release);
    if (engine_thread_.joinable()) {
        engine_thread_.join();
    }
    spdlog::info("AeroMatch worker thread stopped");
}

void MatchingEngine::run() {
    while (running_.load(std::memory_order_acquire)) {
        Order incoming;
        if (!order_buffer_.try_pop(incoming)) {
            std::this_thread::sleep_for(std::chrono::microseconds(20));
            continue;
        }

        std::string symbol(incoming.symbol.data(), config::SYMBOL_LEN);
        size_t null_pos = symbol.find('\0');
        if (null_pos != std::string::npos) symbol.resize(null_pos);

        const SymbolInfo* info = get_symbol(symbol);
        if (!info) {
            metrics_.increment_orders_rejected();
            if (event_tap_) {
                event_tap_->on_order_rejected(incoming.order_id, static_cast<uint8_t>(OrderAcceptance::REJECTED_INVALID_INPUT));
            }
            continue;
        }

        AddOrderResult result;
        {
            std::lock_guard<std::mutex> lock(books_mutex_);
            OrderBook* book = get_or_create_book(symbol);
            if (!book) {
                metrics_.increment_orders_rejected();
                continue;
            }
            result = book->add_order(std::move(incoming));
        }

        if (result.acceptance == OrderAcceptance::ACCEPTED ||
            result.acceptance == OrderAcceptance::ACCEPTED_ZERO_FILL) {
            metrics_.increment_orders_received();
            if (!result.trades.empty()) {
                metrics_.increment_orders_matched();
                metrics_.increment_trades_executed(result.trades.size());
            }
        } else {
            metrics_.increment_orders_rejected();
        }
        if (result.critical_error) {
            metrics_.increment_critical_errors();
        }
    }
}

AddOrderResult MatchingEngine::process_order(Order&& order) {
    std::lock_guard<std::mutex> lock(books_mutex_);

    std::string symbol(order.symbol.data(), config::SYMBOL_LEN);
    size_t null_pos = symbol.find('\0');
    if (null_pos != std::string::npos) symbol.resize(null_pos);

    const SymbolInfo* info = get_symbol(symbol);
    if (!info) {
        metrics_.increment_orders_rejected();
        if (event_tap_) {
            event_tap_->on_order_rejected(order.order_id, static_cast<uint8_t>(OrderAcceptance::REJECTED_INVALID_INPUT));
        }
        return AddOrderResult{OrderAcceptance::REJECTED_INVALID_INPUT, {}, false};
    }

    OrderBook* book = get_or_create_book(symbol);
    if (!book) {
        metrics_.increment_orders_rejected();
        if (event_tap_) {
            event_tap_->on_order_rejected(order.order_id, static_cast<uint8_t>(OrderAcceptance::REJECTED_RATE_LIMITED));
        }
        return AddOrderResult{OrderAcceptance::REJECTED_RATE_LIMITED, {}, false};
    }

    AddOrderResult result = book->add_order(std::move(order));

    if (result.acceptance == OrderAcceptance::ACCEPTED ||
        result.acceptance == OrderAcceptance::ACCEPTED_ZERO_FILL) {
        metrics_.increment_orders_received();
        if (!result.trades.empty()) {
            metrics_.increment_orders_matched();
            metrics_.increment_trades_executed(result.trades.size());
        }
    } else {
        metrics_.increment_orders_rejected();
    }
    if (result.critical_error) {
        metrics_.increment_critical_errors();
    }

    return result;
}

OrderBook* MatchingEngine::get_or_create_book(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it != books_.end()) return &it->second;

    if (books_.size() >= max_symbols_) return nullptr;

    auto [new_it, inserted] = books_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(symbol),
        std::forward_as_tuple(symbol.c_str(), &order_pool_, max_active_orders_per_symbol_, max_active_orders_per_user_, event_tap_)
    );
    if (!inserted) return nullptr;

    return &new_it->second;
}

const OrderBook* MatchingEngine::get_book(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(books_mutex_);
    auto it = books_.find(symbol);
    return it == books_.end() ? nullptr : &it->second;
}

size_t MatchingEngine::active_order_count() const {
    std::lock_guard<std::mutex> lock(books_mutex_);
    size_t total = 0;
    for (const auto& pair : books_) {
        total += pair.second.active_order_count();
    }
    return total;
}

size_t MatchingEngine::pool_used() const {
    return order_pool_.used_count();
}

size_t MatchingEngine::pool_capacity() const {
    return order_pool_.capacity();
}

} // namespace aeromatch
