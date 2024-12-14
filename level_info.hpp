#pragma once

#include "usings.hpp"

struct LevelInfo {
    Price _price;
    Quantity _quantity;
};

using LevelInfos = std::vector<LevelInfo>;