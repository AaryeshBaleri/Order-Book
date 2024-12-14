#pragma once

#include <iostream>
#include <map>
#include <unordered_map>
#include <numeric>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <time.h>
#include <optional>

#include "usings.hpp"
#include "order_book_level_infos.hpp"
#include "order.hpp"
#include "order_modify.hpp"
#include "trade.hpp"

class Orderbook {
private:
    struct OrderEntry {
        OrderPointer _order = nullptr;
        OrderPointers::iterator _location;
    };

    struct LevelData {
        Quantity _quantity{ };
        Quantity _count{ };

        enum class Action {
            ADD,
            REMOVE, 
            MATCH
        };
    };

    std::unordered_map<Price, LevelData> _data;
    std::map<Price, OrderPointers, std::greater<Price>> _bids;
    std::map<Price, OrderPointers, std::less<Price>> _asks;
    std::unordered_map<OrderId, OrderEntry> _orders;
    mutable std::mutex _ordersMutex;
    std::thread _ordersPruneThread;
    std::condition_variable _shutdownConditionVariable;
    std::atomic<bool> _shutdown{ false };

    void pruneGoodForDayOrders() {
        using namespace std::chrono;
        const auto end = hours(16);

        while(true) {
            const auto now = system_clock::now();
            const auto now_c = system_clock::to_time_t(now);
            std::tm* now_parts = localtime(&now_c);

            if(now_parts->tm_hour >= end.count()){
                now_parts->tm_mday += 1;
            }

            now_parts->tm_hour = end.count();
            now_parts->tm_min = 0;
            now_parts->tm_sec = 0;
            auto next = system_clock::from_time_t(mktime(now_parts));
            auto till = next - now + milliseconds(100);

            {
                std::unique_lock ordersLock{ _ordersMutex };
                if(_shutdown.load(std::memory_order_acquire) || 
                _shutdownConditionVariable.wait_for(ordersLock, till) == std::cv_status::no_timeout) {
                    return;
                }
            }

            OrderIds orderIds;

            {
                std::scoped_lock ordersLock{_ordersMutex };
                for(const auto& [_, entry]: _orders) {
                    const auto& [order, ignore] = entry;
                    if(order->getOrderType() != OrderType::GOODFORDAY) {
                        continue;
                    }
                    orderIds.push_back(order->getOrderId());
                }
            }

            cancelOrders(orderIds);

        }
    }

    void cancelOrderInternal(OrderId orderId) {
            if(_orders.find(orderId) == _orders.end()) {
                return;
            }

            const auto& [order, iterator] = _orders.at(orderId);
            _orders.erase(orderId);

            if(order->getSide() == Side::SELL) {
                auto price = order->getPrice();
                auto& orders = _asks.at(price);
                orders.erase(iterator);
                if(orders.empty()) {
                    _asks.erase(price);
                }
            }

            else {
                auto price = order->getPrice();
                auto& orders = _bids.at(price);
                orders.erase(iterator);
                if(orders.empty()) {
                    _bids.erase(price);
                }
            }

            onOrderCancelled(order);
        }

    void onOrderCancelled(OrderPointer order) {
        updateLevelData(order->getPrice(), order->getRemainingQuantity(), LevelData::Action::REMOVE);
    }

    void onOrderAdded(OrderPointer order) {
        updateLevelData(order->getPrice(), order->getRemainingQuantity(), LevelData::Action::ADD);
    }

    void onOrderMatched(Price price, Quantity quantity, bool isFullyFilled) {
        updateLevelData(price, quantity, isFullyFilled ? LevelData::Action::REMOVE : LevelData::Action::MATCH);
    }

    void updateLevelData(Price price, Quantity quantity, LevelData::Action action) {
        auto& data = _data[price];
        data._count += action == LevelData::Action::REMOVE? -1: action == LevelData::Action::ADD? 1: 0;
        if(action == LevelData::Action::REMOVE || action == LevelData::Action::MATCH) {
            data._quantity -= quantity;
        }

        else {
            data._quantity += quantity;
        }

        if(data._count == 0) {
            _data.erase(price);
        }
    }

    bool canFullyFill(Side side, Price price, Quantity quantity) const {
        if(!canMatch(side, price)) {
            return false;
        }

        std::optional<Price> threshold;
        if(side == Side::BUY) {
            const auto [askPrice, _] = *_asks.begin();
            threshold = askPrice;
        }
        else {
            const auto [bidPrice, _] = *_bids.begin();
            threshold = bidPrice;
        }

        for(const auto& [levelPrice, levelData]: _data) {
            if(threshold.has_value() &&
            (side == Side::BUY && threshold.value() > levelPrice) ||
            (side == Side::SELL && threshold.value() < levelPrice)) {
                continue;
            }

            if((side == Side::BUY && levelPrice > price) ||
            (side == Side::SELL && levelPrice < price)) {
                continue;
            }

            if(quantity <= levelData._quantity) {
                return true;
            }

            quantity -= levelData._quantity;
        }
        return false;
    }

    void cancelOrders(OrderIds orderIds) {
        std::scoped_lock ordersLock{ _ordersMutex };

        for(const auto& orderId: orderIds) {
            cancelOrderInternal(orderId);
        }
    }

    bool canMatch(Side side, Price price) const {
        if(side == Side::BUY) {
            if(_asks.empty()) {
                return false;
            }

            Price bestAsk = _asks.begin()->first;
            return price >= bestAsk;
        }

        else {
            if(_bids.empty()) {
                return false;
            }
            Price bestBid = _bids.begin()->first;
            return price <= bestBid;
        }
    }

    Trades matchOrders() {
        Trades trades;
        trades.reserve(_orders.size());
        while(true) {
            if(_bids.empty() || _asks.empty()) {
                break;
            }

            auto& [bidPrice, bids] = *_bids.begin();
            auto& [askPrice, asks] = *_asks.begin();

            if(bidPrice < askPrice) {
                break;
            }

            while(bids.size() && asks.size()) {
                auto& bid = bids.front();
                auto& ask = asks.front();

                Quantity quantity = std::min(bid->getRemainingQuantity(), ask->getRemainingQuantity());
                bid->fill(quantity);
                ask->fill(quantity);

                if(bid->isFilled()) {
                    bids.pop_front();
                    _orders.erase(bid->getOrderId());
                }

                if(ask->isFilled()) {
                    asks.pop_front();
                    _orders.erase(ask->getOrderId());
                }

                trades.push_back(Trade{ 
                    TradeInfo{ bid->getOrderId(), bid->getPrice(), quantity },
                    TradeInfo{ ask->getOrderId(), ask->getPrice(), quantity }
                    });

                onOrderMatched(bid->getPrice(), quantity, bid->isFilled());
                onOrderMatched(ask->getPrice(), quantity, ask->isFilled());
            }

            if(bids.empty()) {
                _bids.erase(bidPrice);
                _data.erase(bidPrice);
            }

            if(asks.empty()) {
                _asks.erase(askPrice);
                _data.erase(askPrice);
            }
        }

        if(!_bids.empty()) {
            auto& [_, bids] = *_bids.begin();
            auto& order = bids.front();
            if(order->getOrderType() == OrderType::FILLANDKILL) {
                cancelOrder(order->getOrderId());
            }
        }

        if(!_asks.empty()) {
            auto& [_, asks] = *_asks.begin();
            auto& order = asks.front();
            if(order->getOrderType() == OrderType::FILLANDKILL) {
                cancelOrder(order->getOrderId());
            }
        }

        return trades;
    }

    public:
        Orderbook() : _ordersPruneThread{ [this] { pruneGoodForDayOrders(); } } { }

        ~Orderbook() {
            _shutdown.store(true, std::memory_order_release);
	        _shutdownConditionVariable.notify_one();
	        _ordersPruneThread.join();
        }

        Trades addOrder(OrderPointer order) {
            std::scoped_lock ordersLock {_ordersMutex };
            
            if(_orders.find(order->getOrderId()) != _orders.end()) {
                return { };
            }

            if(order->getOrderType() == OrderType::MARKET) {
                if(order->getSide() == Side::BUY && !_asks.empty()) {
                    const auto& [worstAsk, _] = *_asks.rbegin();
                    order->toGoodTillCancel(worstAsk);
                }

                else if(order->getSide() == Side::SELL && !_bids.empty()) {
                    const auto& [worstBid, _] = *_bids.rbegin();
                    order->toGoodTillCancel(worstBid);
                }

                else {
                    return { };
                }
            }

            if(order->getOrderType() == OrderType::FILLANDKILL && !canMatch(order->getSide(), order->getPrice())) {
                return { };
            }

            if(order->getOrderType() == OrderType::FILLORKILL && !canFullyFill(order->getSide(), order->getPrice(), order->getInitialQuantity())) {
                return { };
            }

            OrderPointers::iterator iterator;
            if(order->getSide() == Side::BUY) {
                auto& orders = _bids[order->getPrice()];
                orders.push_back(order);
                iterator = std::next(orders.begin(), orders.size() - 1);  
            }
            else {
                auto& orders = _asks[order->getPrice()];
                orders.push_back(order);
                iterator = std::next(orders.begin(), orders.size() - 1);
            }

            _orders.insert({ order->getOrderId(), OrderEntry{ order, iterator } });
            
            onOrderAdded(order);
            
            return matchOrders();
        } 

        void cancelOrder(OrderId orderId) {
            std::scoped_lock ordersLock{ _ordersMutex };
            cancelOrderInternal(orderId);
        }

        Trades matchOrder(OrderModify order) {
            if(_orders.find(order.getOrderId()) == _orders.end()) {
                return { };
            }

            const auto& [existingOrder, _] = _orders.at(order.getOrderId());
            cancelOrder(order.getOrderId());
            return addOrder(order.toOrderPointer(existingOrder->getOrderType()));
        } 

        std::size_t size() const { return _orders.size(); }

        OrderbookLevelInfos getOrderInfo() const {
            LevelInfos bidInfos, askInfos;
            bidInfos.reserve(_orders.size());
            askInfos.reserve(_orders.size());

            auto createLevelInfos = [](Price price, const OrderPointers& orders) {
                return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
                [](Quantity runningSum, const OrderPointer& order) {
                    return runningSum + order->getRemainingQuantity(); 
                }) };
            };

            for(const auto& [price, orders]: _bids) {
                bidInfos.push_back(createLevelInfos(price, orders));
            }

            for(const auto& [price, orders]: _asks) {
                askInfos.push_back(createLevelInfos(price, orders));
            }

            return OrderbookLevelInfos{ bidInfos, askInfos };
        }
};


