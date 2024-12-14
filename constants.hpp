#pragma once

#include <limits>

#include "usings.hpp"

struct Constants {
    static const Price invalidPrice = std::numeric_limits<Price>::quiet_NaN();
};
