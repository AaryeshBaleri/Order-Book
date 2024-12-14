#pragma once

#include "order.hpp"

class OrderModify {
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity) {
        _orderId = orderId;
        _price = price;
        _side = side;
        _quantity = quantity;
    }

    OrderId getOrderId() const { return _orderId; }
    Price getPrice() const { return _price; }
    Side getSide() const { return _side; }
    Quantity getQuantity() const { return _quantity; }

    OrderPointer toOrderPointer(OrderType type) const {
        return std::make_shared<Order>(type, _orderId, _side, _price, _quantity);
    }

private:
    OrderId _orderId;
    Price _price;
    Side _side;
    Quantity _quantity;
};