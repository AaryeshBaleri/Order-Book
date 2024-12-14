#pragma once

#include "level_info.hpp"

class OrderbookLevelInfos {
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks) {
        _bids = bids;
        _asks = asks;
    }

    const LevelInfos& getBids() const { return _bids; }
    const LevelInfos& getAsks() const { return _asks; }

private:
    LevelInfos _bids;
    LevelInfos _asks;
};