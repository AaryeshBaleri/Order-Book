#pragma once

#include <list>
#include <string>
#include <stdexcept>
#include <memory>

#include "order_type.hpp"
#include "side.hpp"
#include "usings.hpp"
#include "constants.hpp"

class Order {
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) {
        _orderType = orderType;
        _orderId = orderId;
        _side = side;
        _price = price;
        _initialQuantity = quantity;
        _remainingQuantity = quantity;
    }

    Order(OrderId orderId, Side side, Quantity quantity)
    : Order(OrderType::MARKET, orderId, side, Constants::invalidPrice, quantity) 
    { }

    OrderId getOrderId() const { return _orderId ; }
    Side getSide() const { return _side; }
    Price getPrice() const { return _price; }
    OrderType getOrderType() const { return _orderType ; }
    Quantity getInitialQuantity() const { return _initialQuantity; }
    Quantity getRemainingQuantity() const { return _remainingQuantity; }
    Quantity getFilledQuantity() const { return getInitialQuantity() - getRemainingQuantity(); }
    bool isFilled() const { return  getRemainingQuantity() == 0; }
    void fill(Quantity quantity) {
        if(quantity > getRemainingQuantity()) {
            throw std::logic_error("Order " + std::to_string(getOrderId()) + " cannot be filled for more than its remaining quantity.");
        }
        _remainingQuantity -= quantity;
    }

    void toGoodTillCancel(Price price) {
        if(getOrderType() != OrderType::MARKET) {
            throw std::logic_error("Order " + std::to_string(getOrderId()) + " cannot have its price adjusted, only market orders can.");
        }

        _price = price;
        _orderType = OrderType::GOODTILLCANCEL;
    }

private:
    OrderType _orderType;
    OrderId _orderId;
    Side _side;
    Price _price;
    Quantity _initialQuantity;
    Quantity _remainingQuantity;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;