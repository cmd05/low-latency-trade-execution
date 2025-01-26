#pragma once

#include <thread>

#include <api/trade_handler.h>
#include <lib/utilities.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define DERIBIT_JSON_RPC    "2.0"
#define DERIBIT_DEFAULT_REQUEST_ID  1

class deribit : public trade_handler {
public:
    deribit(): trade_handler("wss://test.deribit.com/ws/api/v2") {}
    ~deribit() {}

    websocketpp::lib::error_code auth() override {
        json request;

        request["method"] = "public/auth";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;
        
        request["params"] = json::object();
        request["params"]["grant_type"] = "client_credentials";
        request["params"]["client_id"] = m_key.id;
        request["params"]["client_secret"] = m_key.secret;

        websocket_endpoint::send_result result = m_endpoint->send(m_con_id, request.dump());

        if(!result.ec) {
            // wait for response from the endpoint and store the access token
            // wait upto a maximum specified time

            using namespace std::chrono_literals;
            auto thread_sleep_time = 100ms;
            auto max_wait_time = 5000; // 5 seconds

            json json_response;
            auto msg = m_endpoint->get_latest_message(m_con_id);
            auto start_time = std::chrono::high_resolution_clock::now();

            while(true) {
                // add a thread sleep to avoid excessively polling get_latest_message
                std::this_thread::sleep_for(thread_sleep_time);

                auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
                if(elapsed_time > max_wait_time) {
                    APP_LOG(log_flags::trade_handler, "Authentication response time exceeded " << max_wait_time << "ms");
                    return result.ec;
                }
    
                auto tmp = m_endpoint->get_latest_message(m_con_id);
                if(tmp == msg) continue; // check if new message has arrived
                msg = tmp;

                if(!msg || msg->length() <= WS_MSG_TYPE_LEN) continue; // check if new message is valid

                // parse the message and store the access_token if the message is valid
                json_response = json::parse(msg->substr(WS_MSG_TYPE_LEN));
                if(json_response.contains("result") && json_response["result"].contains("access_token")) {
                    m_access_token = json_response["result"]["access_token"];
                    break;
                }
            }

            APP_LOG(log_flags::trade_handler, "(deribit) access token: " << m_access_token);
        }

        return result.ec;
    }

    /**
     * @brief Subscribe to one or more channels.
     * 
     * params["channels"] (true) - A list of channels to subscribe to.
     *
     */
    void subscribe(trade_handler::subscriptions_params params) {
        static constexpr unsigned int max_label_len = 16;

        json request;
        request["params"] = json::object();

        if(params.channels.size() == 0) {
            APP_LOG(log_flags::trade_handler, "(deribit) Specify one or more channels to subscribe");
            return;
        }

        request["params"]["channels"] = params.channels;

        request["method"] = "private/subscribe";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;

        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Unsubscribe from all the channels subscribed so far.
     */
    void unsubscribe_all() {
        json request;

        request["params"] = json::object();
        request["method"] = "private/unsubscribe_all";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;

        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Gracefully close websocket connection, when COD (Cancel On Disconnect) is enabled orders are not cancelled
     * 
     * params["invalidate_token"] (false) - If value is true all tokens created in current session are invalidated.
     *                                          default: true
     */
    void logout(trade_handler::logout_params params) {
        json request;

        request["params"] = json::object();
        request["params"]["invalidate_token"] = params.invalidate_token;

        request["method"] = "public/get_time";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;
        

        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Retrieves the current time (in milliseconds). 
     * This API endpoint can be used to check the clock skew between your software and Deribit's systems.
     */
    void test() override {
        json request;

        request["method"] = "public/get_time";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;
        
        request["params"] = json::object();

        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Retrieves the order book, along with other market values for a given instrument.
     * @param params
     *   params["instrument_name"] (true) - The instrument name for which to retrieve the order book
     *   params["depth"] (false) - The number of entries to return for bids and asks.
     *      caller must specify depth as -1, if not specifying depth
     */
    void get_order_book(trade_handler::order_book_params params) override {
        static constexpr std::array allowed_depths = {1, 5, 10, 20, 50, 100, 1000, 10000};

        json request;
        request["params"] = json::object();
        
        if(params.instrument.empty()) {
            APP_LOG(log_flags::trade_handler, "(deribit) instrument not specified");
            return;
        }
        request["params"]["instrument_name"] = params.instrument;

        if(params.depth != -1) {
            if(std::find(allowed_depths.begin(), allowed_depths.end(), params.depth) == allowed_depths.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid depth specified: " << params.depth);
                return;
            }

            request["params"]["depth"] = params.depth;
        }

        request["method"] = "public/get_order_book";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;

        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Retrieve user positions
     * @param params
     *   params[<name>] (<reqd>)
     *   params["currency"] (false)
     *   params["kind"] (false) - Kind filter on positions
     */
    void get_positions(trade_handler::positions_params params) override {
        static constexpr std::array allowed_currency = {"BTC", "ETH", "USDC", "USDT", "EURR", "any"};
        static constexpr std::array allowed_kind = {"future", "option", "spot", "future_combo", "option_combo"};

        json request;
        request["params"] = json::object();

        // specify currency only if valid
        if(!params.currency.empty()) {
            if(std::find(allowed_currency.begin(), allowed_currency.end(), params.currency) == allowed_currency.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid currency specified: " << params.currency);
                return;
            }

            request["params"]["currency"] = params.currency;
        }

        if(!params.kind.empty()) {
            if(std::find(allowed_kind.begin(), allowed_kind.end(), params.kind) == allowed_kind.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid kind specified: " << params.kind);
                return;
            }

            request["params"]["kind"] = params.kind;
        }

        // specify request details
        request["method"] = "private/get_positions";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;
        
        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Places a buy order for an instrument.
     * @param params
     *   -> All string values are empty for not specified
     *   -> All integral values are -1 for not specified
     * 
     *   params[<name>] (<reqd>)
     *   params["instrument_name"] (true)
     *   params["amount"] (false) - It represents the requested order size.
     *   params["contracts"] (false) - It represents the requested order size in contract units and can be passed instead of `amount`
     *   params["type"] (false) - The order type, default: "limit"
     *   params["label"] (false) - User defined label for the order (maximum 64 characters)
     *   params["price"] (false) - The order price in base currency (Only for limit and stop_limit orders)
     *   params["time_in_force"] (false) - Specifies how long the order remains in effect. Default "good_til_cancelled"
     *   params["trigger"] (false) - Defines the trigger type. Required for "Stop-Loss", "Take-Profit" and "Trailing" trigger orders
     *   params["trigger_price"] (false) - Trigger price, required for trigger orders only
     */
    void buy(trade_handler::order_params params) override {
        static constexpr std::array allowed_types = {"limit", "stop_limit", "take_limit", "market", "stop_market", "take_market", "market_limit", "trailing_stop"};
        static constexpr std::array allowed_time_in_force = {"good_til_cancelled", "good_til_day", "fill_or_kill", "immediate_or_cancel"};
        static constexpr std::array allowed_triggers = {"index_price", "mark_price", "last_price"};
        static constexpr unsigned int max_label_len = 64;

        json request;
        request["params"] = json::object();

        if(params.instrument.empty()) {
            APP_LOG(log_flags::trade_handler, "(deribit) instrument not specified");
            return;
        }
        request["params"]["instrument_name"] = params.instrument;

        if(params.amount == -1 && params.contracts == -1) {
            APP_LOG(log_flags::trade_handler, "(deribit) Must specify atleast amount or contracts");
            return;
        }

        if(params.amount != -1 && params.contracts != -1 && params.amount != params.contracts) {
            APP_LOG(log_flags::trade_handler, "(deribit) amount and contracts must match");
            return;
        }

        if(params.amount != -1)
            request["params"]["amount"] = params.amount;
        if(params.contracts != -1)
            request["params"]["contracts"] = params.contracts;

        if(!params.type.empty()) {
            if(std::find(allowed_types.begin(), allowed_types.end(), params.type) == allowed_types.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid type specified: " << params.type);
                return;
            }

            request["params"]["type"] = params.type;
        }

        if(!params.label.empty()) {
            if(params.label.length() > max_label_len) {
                APP_LOG(log_flags::trade_handler, "(deribit) label length exceeds " << max_label_len << " characters");
                return;
            }

            request["params"]["label"] = params.label;
        }

        if(params.price != -1) {
            request["params"]["price"] = params.price;
        }

        if(!params.time_in_force.empty()) {
            if(std::find(allowed_time_in_force.begin(), allowed_time_in_force.end(), params.time_in_force) == allowed_time_in_force.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid time_in_force specified: " << params.time_in_force);
                return;
            }

            request["params"]["time_in_force"] = params.time_in_force;
        }

        if(!params.trigger.empty()) {
            if(std::find(allowed_triggers.begin(), allowed_triggers.end(), params.trigger) == allowed_triggers.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid trigger specified: " << params.trigger);
                return;
            }

            if(params.trigger_price == -1) {
                APP_LOG(log_flags::trade_handler, "(deribit) Trigger price must be specified for trigger orders.");
                return;
            }

            request["params"]["trigger"] = params.trigger;
            request["params"]["trigger_price"] = params.trigger_price;
        }

        // specify request details
        request["method"] = "private/buy";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;

        APP_LOG(log_flags::trade_handler, "(deribit) Buy order request sent. Check details");
        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Places a buy order for an instrument.
     * @param params
     *   -> All string values are empty for not specified
     *   -> All integral values are -1 for not specified
     * 
     *   params[<name>] (<reqd>)
     *   params["instrument_name"] (true)
     *   params["amount"] (false) - It represents the requested order size.
     *   params["contracts"] (false) - It represents the requested order size in contract units and can be passed instead of `amount`
     *   params["type"] (false) - The order type, default: "limit"
     *   params["label"] (false) - User defined label for the order (maximum 64 characters)
     *   params["price"] (false) - The order price in base currency (Only for limit and stop_limit orders)
     *   params["time_in_force"] (false) - Specifies how long the order remains in effect. Default "good_til_cancelled"
     *   params["trigger"] (false) - Defines the trigger type. Required for "Stop-Loss", "Take-Profit" and "Trailing" trigger orders
     *   params["trigger_price"] (false) - Trigger price, required for trigger orders only
     */
    void sell(trade_handler::order_params params) override {
        static constexpr std::array allowed_types = {"limit", "stop_limit", "take_limit", "market", "stop_market", "take_market", "market_limit", "trailing_stop"};
        static constexpr std::array allowed_time_in_force = {"good_til_cancelled", "good_til_day", "fill_or_kill", "immediate_or_cancel"};
        static constexpr std::array allowed_triggers = {"index_price", "mark_price", "last_price"};
        static constexpr unsigned int max_label_len = 64;

        json request;
        request["params"] = json::object();

        if(params.instrument.empty()) {
            APP_LOG(log_flags::trade_handler, "(deribit) instrument not specified");
            return;
        }
        request["params"]["instrument_name"] = params.instrument;

        if(params.amount == -1 && params.contracts == -1) {
            APP_LOG(log_flags::trade_handler, "(deribit) Must specify atleast amount or contracts");
            return;
        }

        if(params.amount != -1 && params.contracts != -1 && params.amount != params.contracts) {
            APP_LOG(log_flags::trade_handler, "(deribit) amount and contracts must match");
            return;
        }

        if(params.amount != -1)
            request["params"]["amount"] = params.amount;
        if(params.contracts != -1)
            request["params"]["contracts"] = params.contracts;

        if(!params.type.empty()) {
            if(std::find(allowed_types.begin(), allowed_types.end(), params.type) == allowed_types.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid type specified: " << params.type);
                return;
            }

            request["params"]["type"] = params.type;
        }

        if(!params.label.empty()) {
            if(params.label.length() > max_label_len) {
                APP_LOG(log_flags::trade_handler, "(deribit) label length exceeds " << max_label_len << " characters");
                return;
            }

            request["params"]["label"] = params.label;
        }

        if(params.price != -1) {
            request["params"]["price"] = params.price;
        }

        if(!params.time_in_force.empty()) {
            if(std::find(allowed_time_in_force.begin(), allowed_time_in_force.end(), params.time_in_force) == allowed_time_in_force.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid time_in_force specified: " << params.time_in_force);
                return;
            }

            request["params"]["time_in_force"] = params.time_in_force;
        }

        if(!params.trigger.empty()) {
            if(std::find(allowed_triggers.begin(), allowed_triggers.end(), params.trigger) == allowed_triggers.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid trigger specified: " << params.trigger);
                return;
            }

            if(params.trigger_price == -1) {
                APP_LOG(log_flags::trade_handler, "(deribit) Trigger price must be specified for trigger orders.");
                return;
            }

            request["params"]["trigger"] = params.trigger;
            request["params"]["trigger_price"] = params.trigger_price;
        }

        // specify request details
        request["method"] = "private/sell";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;

        APP_LOG(log_flags::trade_handler, "(deribit) Buy order request sent. Check details");
        m_endpoint->send(m_con_id, request.dump());
    }



    /**
     * @brief Change price, amount and/or other properties of an order.
     * @param params
     *   -> All string values are empty for not specified
     *   -> All integral values are -1 for not specified
     * 
     *   params[<name>] (<reqd>)
     *   params["order_id"] (true)
     *   params["amount"] (false) - It represents the requested order size.
     *   params["contracts"] (false) - It represents the requested order size in contract units and can be passed instead of `amount`
     *   params["price"] (false) - The order price in base currency (Only for limit and stop_limit orders)
     *   params["trigger_price"] (false) - Trigger price, required for trigger orders only
     */
    void edit(trade_handler::order_params params) override {
        json request;
        request["params"] = json::object();

        if(params.order_id.empty()) {
            APP_LOG(log_flags::trade_handler, "(deribit) Order ID not specified");
            return;
        }
        request["params"]["order_id"] = params.order_id;

        if(params.amount == -1 && params.contracts == -1) {
            APP_LOG(log_flags::trade_handler, "(deribit) Must specify atleast amount or contracts");
            return;
        }

        if(params.amount != -1 && params.contracts != -1 && params.amount != params.contracts) {
            APP_LOG(log_flags::trade_handler, "(deribit) amount and contracts must match");
            return;
        }

        if(params.amount != -1)
            request["params"]["amount"] = params.amount;
        if(params.contracts != -1)
            request["params"]["contracts"] = params.contracts;
        if(params.price != -1)
            request["params"]["price"] = params.price;
        if(params.trigger_price != -1)
            request["params"]["trigger_price"] = params.trigger_price;


        // specify request details
        request["method"] = "private/edit";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;

        APP_LOG(log_flags::trade_handler, "(deribit) Edit order request sent. Check details");
        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Cancel an order, specified by order id
     * @param params
     *   params["order_id"] (true)
     */
    void cancel(order_params params) {
        json request;

        if(params.order_id.empty()) {
            APP_LOG(log_flags::trade_handler, "Order ID must be specified");
            return;
        }

        request["params"] = { {"order_id", params.order_id} };

        request["method"] = "private/cancel";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;

        m_endpoint->send(m_con_id, request.dump());
    }

    /**
     * @brief Retrieves list of user's open orders across many currencies.
     * @param params
     *   params["kind"] (false) - Instrument kind, if not provided instruments of all kinds are considered
     *   params["type"] (false) - Order type, default - all
     */
    virtual void get_open_orders(trade_handler::open_orders_params params) {
        static constexpr std::array allowed_kinds = {"future", "option", "spot", "future_combo", "option_combo"};
        static constexpr std::array allowed_types = {"all", "limit", "trigger_all", "stop_all",
            "stop_limit", "stop_market", "take_all", "take_limit", "take_market", "trailing_all", "trailing_stop"};

        json request;
        request["params"] = json::object();

        if(!params.kind.empty()) {
            if(std::find(allowed_kinds.begin(), allowed_kinds.end(), params.kind) == allowed_kinds.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid kind specified: " << params.kind);
                return;
            }

            request["params"]["kind"] = params.kind;
        }

        if(!params.type.empty()) {
            if(std::find(allowed_types.begin(), allowed_types.end(), params.type) == allowed_types.end()) {
                APP_LOG(log_flags::trade_handler, "(deribit) invalid type specified: " << params.type);
                return;
            }

            request["params"]["type"] = params.type;
        }

        request["method"] = "private/get_open_orders";
        request["jsonrpc"] = DERIBIT_JSON_RPC;
        request["id"] = DERIBIT_DEFAULT_REQUEST_ID;

        m_endpoint->send(m_con_id, request.dump());
    }
private:
    std::string m_access_token;
};