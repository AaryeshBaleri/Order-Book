#pragma once

#include "trade_info.hpp"

class Trade {
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade): _bidTrade{bidTrade}, _askTrade{askTrade} {}

    const TradeInfo& getBidTrade() const { return _bidTrade; }
    const TradeInfo& getAskTrade() const { return _askTrade; }

private:
    const TradeInfo& _bidTrade;
    const TradeInfo& _askTrade;
};

using Trades = std::vector<Trade>;