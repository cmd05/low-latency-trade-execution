#pragma once

#include <websocket/websocket.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class trade_handler {
public:
    // We can specify additional fields for a different API in the structs

    struct api_key {
        std::string id;
        std::string secret;
    };

    // order_params can be used for buy, sell, edit and cancel orders
    struct order_params {
        float amount;
        float contracts;
        float price;
        float trigger_price;

        std::string instrument;
        std::string type;
        std::string label;
        std::string time_in_force;
        std::string trigger;

        // only for edit and cancel orders
        std::string order_id;
    };

    struct open_orders_params {
        std::string kind;
        std::string type;
    };

    struct order_book_params {
        std::string instrument;
        int depth;
    };

    struct positions_params {
        std::string currency;
        std::string kind;
    };

    struct logout_params {
        bool invalidate_token = true;
    };

    struct subscriptions_params {
        std::vector<std::string> channels;
    };

public:    
    trade_handler(std::string url): m_url{url} {}
    virtual ~trade_handler() {}

    virtual void init(websocket_endpoint* endpoint, api_key key) {
        m_endpoint = endpoint;
        m_key = key;
    }

    // common trade methods which must be implemented
    con_id_type connect() { m_con_id = m_endpoint->connect(m_url); return m_con_id; }
    virtual websocketpp::lib::error_code auth() = 0;
    virtual void test() = 0;

    // these methods may or may not be implemented by derived classes
    virtual void buy(order_params params) {}
    virtual void sell(order_params params) {}
    virtual void edit(order_params params) {}
    virtual void cancel(order_params params) {}
    virtual void get_open_orders(open_orders_params params) {}
    virtual void get_order_book(order_book_params params) {}
    virtual void get_positions(positions_params params) {}

    virtual void subscribe(subscriptions_params params) {}
    virtual void unsubscribe_all() {}

    virtual void logout(logout_params params) {}

protected:
    const std::string m_url;
    websocket_endpoint* m_endpoint;
    api_key m_key;
    con_id_type m_con_id;
};