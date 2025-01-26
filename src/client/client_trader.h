#pragma once

#include <websocket/websocket.h>
#include <api/trade_handler.h>

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr int default_trade_con_id = -1;

class client_trader {
public:
    client_trader(trade_handler* trade_handler_, trade_handler::api_key key);

    con_id_type connect_trade_api();
    void trade_api_auth();
    void test_trade_api();

    void buy(trade_handler::order_params params);
    void sell(trade_handler::order_params params);
    void edit(trade_handler::order_params params);
    void cancel(trade_handler::order_params params);
    void get_open_orders(trade_handler::open_orders_params params);
    void get_order_book(trade_handler::order_book_params params);
    void get_positions(trade_handler::positions_params params);

    void subscribe(trade_handler::subscriptions_params params);
    void unsubscribe_all();

    void logout(trade_handler::logout_params params);

    void print_trade_messages();
private:
    void trade_handler_init();

private:
    websocket_endpoint m_endpoint;

    con_id_type m_trade_api_con_id = default_trade_con_id;
    trade_handler::api_key m_key;

    bool m_trade_api_connected = false;
    bool m_trade_api_auth = false;
    trade_handler* m_trade_handler;
};