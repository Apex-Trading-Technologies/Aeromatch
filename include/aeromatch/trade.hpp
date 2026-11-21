#pragma once

#include <cstdint>
#include <string>
#include <array>

namespace aeromatch {

struct Trade {
    uint64_t trade_id{0};
    uint64_t buy_order_id{0};
    uint64_t sell_order_id{0};
    int64_t price{0};
    int64_t quantity{0};
    uint64_t timestamp_ns{0};
    std::string symbol;
    std::string side;
    std::array<unsigned char, 16> user_id{};
};

} // namespace aeromatch
