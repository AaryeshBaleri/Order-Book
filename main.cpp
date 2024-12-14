#include "order_book.hpp"

int main() {
    Orderbook orderbook;
    orderbook.addOrder(std::make_shared<Order>(OrderType::GOODTILLCANCEL, 1, Side::BUY, 100, 10));
    std::cout << orderbook.size() << std::endl;
    orderbook.addOrder(std::make_shared<Order>(OrderType::GOODTILLCANCEL, 2, Side::SELL, 100, 10));
    std::cout << orderbook.size() << std::endl;
    orderbook.addOrder(std::make_shared<Order>(OrderType::GOODTILLCANCEL, 1, Side::BUY, 100, 10));
    std::cout << orderbook.size() << std::endl;
    orderbook.cancelOrder(1);
    std::cout << orderbook.size() << std::endl;
    return 0;
}