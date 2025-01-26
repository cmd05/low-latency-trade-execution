#include <iostream>
#include <fstream>
#include <string>
#include <memory>

#include <websocket/websocket.h>
#include <api/trade_handler.h>
#include <api/deribit.h>
#include <client/client_trader.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <chrono>
#include <lib/utilities.h>
#include <lib/benchmark.h>

std::chrono::time_point<std::chrono::high_resolution_clock> g_timer_start;
benchmark g_benchmark {"g_benchmark"};

void load_keys(std::string filename, trade_handler::api_key& key) {
    std::ifstream ifs (filename);
    json data = json::parse(ifs);
    
    key.id = data["client_id"];
    key.secret = data["client_secret"];
}

template<typename T>
void read_var(T& var) {
    std::string s;
    std::getline(std::cin, s);

    std::stringstream ss {s};
    ss >> var;

    std::cin.clear();
}

void show_help_text() {
    static constexpr int cmd_width = 50;
    std::ios_base::fmtflags original_flags = std::cout.flags();

    std::cout << std::left; // Align the text to the left
    std::cout
        << std::setw(cmd_width) << "help"
        << "Display this help text\n"

        << std::setw(cmd_width) << "deribit_connect"
        << "Connect to Deribit\n"

        << std::setw(cmd_width) << "deribit_test"
        << "Retrieves the current time (in milliseconds) to check clock skew\n"

        << std::setw(cmd_width) << "deribit_auth"
        << "Authenticate with Deribit\n"

        << std::setw(cmd_width) << "deribit_show"
        << "Show communication with Deribit\n"

        << std::setw(cmd_width) << "deribit_order_book [instrument_name] [depth]"
        << "Retrieves the order book, along with other market values for a given instrument\n"
        
        << std::setw(cmd_width) << "deribit_positions [currency] [kind]"
        << "Retrieve user positions\n"
        << std::setw(cmd_width) << " "
        << "\tcurrency: BTC ETH USDC USDT EURR any\n"
        << std::setw(cmd_width) << " "
        << "\tkind: future option spot future_combo option_combo\n"

        << std::setw(cmd_width) << "deribit_buy"
        << "Places a buy order for an instrument in interactive command-line mode\n"

        << std::setw(cmd_width) << "deribit_sell"
        << "Places a sell order for an instrument in interactive command-line mode\n"
        
        << std::setw(cmd_width) << "deribit_edit"
        << "Change price, amount and/or other properties of an order in interactive command-line mode\n"
        
        << std::setw(cmd_width) << "deribit_cancel [order_id]"
        << "Cancel an order, specified by order id\n"

        << std::setw(cmd_width) << "deribit_open_orders [kind] [type]"
        << "Retrieves list of user's open orders across many currencies\n"
        << std::setw(cmd_width) << " "
        << "\tkind: future option spot future_combo option_combo\n"
        << std::setw(cmd_width) << " "
        << "\ttype: all limit trigger_all stop_all stop_limit stop_market take_all take_limit\n"
        << std::setw(cmd_width) << " "
        << "\t\ttake_market trailing_all trailing_stop\n"

        << std::setw(cmd_width) << "deribit_sub [channels...]"
        << "Subscribe to one or more channels\n"

        << std::setw(cmd_width) << "deribit_unsub"
        << "Unsubscribe from all the channels subscribed so far\n"

        << std::setw(cmd_width) << "deribit_logout [<bool> invalidate_token]"
        << "Gracefully close websocket connection\n"

        << std::setw(cmd_width) << "quit"
        << "Exit the program\n";

    std::cout.flags(original_flags); // restore original flags
}

void order_interface(trade_handler::order_params& params, std::string& order_type) {
    std::cout << "Enter " << order_type << " order details\n";

    params.amount = params.contracts = params.price = params.trigger_price = -1;

    if(order_type != "edit") {
        std::cout << "Instrument: ";
        read_var(params.instrument);
    } else {
        std::cout << "Order ID: ";
        read_var(params.order_id);
    }

    std::cout << "Amount: ";
    read_var(params.amount);

    std::cout << "Contracts: ";
    read_var(params.contracts);

    std::cout << "Price: ";
    read_var(params.price);

    if(order_type != "edit") {
        std::cout << "Type: ";
        read_var(params.type);

        std::cout << "Label: ";
        read_var(params.label);

        std::cout << "Time in force: ";
        read_var(params.time_in_force);

        std::cout << "Trigger: ";
        read_var(params.trigger);
    }

    std::cout << "Trigger Price: ";
    read_var(params.trigger_price);
}

int main() {
    // start global timer
    g_timer_start = std::chrono::high_resolution_clock::now();

    trade_handler::api_key key;
    load_keys("api_key.json", key);

    std::unique_ptr<deribit> deribit_uptr = std::make_unique<deribit>();
    trade_handler* deribit_handler = deribit_uptr.get();

    client_trader trader {deribit_handler, key};

    bool done = false;
    std::string input;

    while (!done) {
        std::cout << "Enter Command: ";
        std::getline(std::cin, input);

        if (input == "quit") {
            done = true;

        } else if (input == "help") {
            show_help_text();

        } else if (input.substr(0,15) == "deribit_connect") {
            con_id_type id = trader.connect_trade_api();
            
            if (id != -1) {
                std::cout << "Created connection with id " << id << std::endl;
            }

        } else if (input.substr(0,12) == "deribit_auth") {
            g_benchmark.reset("e2e_auth_request_benchmark");
            g_benchmark.start();

            trader.trade_api_auth();

        } else if (input.substr(0,17) == "deribit_positions") {
            std::string cmd;
            trade_handler::positions_params params;

            std::stringstream ss{input};
            ss >> cmd >> params.currency >> params.kind;

            trader.get_positions(params);

        } else if (input.substr(0,11) == "deribit_buy" || input.substr(0,12) == "deribit_sell" 
                    || input.substr(0,12) == "deribit_edit") {
            // Use the same interface for buy, sell and edit orders
            trade_handler::order_params params;
            std::string order_type;

            if(input.substr(0,11) == "deribit_buy")
                order_type = "buy";
            else if(input.substr(0,12) == "deribit_sell")
                order_type = "sell";
            else if(input.substr(0,12) == "deribit_edit")
                order_type = "edit";

            order_interface(params, order_type);

            g_benchmark.reset("e2e_" + order_type + "_order_" + "benchmark");
            g_benchmark.start();

            if(order_type == "buy")
                trader.buy(params);
            else if(order_type == "sell")
                trader.sell(params);
            else
                trader.edit(params);

        } else if (input.substr(0,14) == "deribit_cancel") {
            std::string cmd;
            trade_handler::order_params params;

            std::stringstream ss{input};
            ss >> cmd >> params.order_id;

            trader.cancel(params);

        } else if (input.substr(0,19) == "deribit_open_orders") {
            std::string cmd;
            trade_handler::open_orders_params params;

            std::stringstream ss{input};
            ss >> cmd >> params.kind >> params.type;

            trader.get_open_orders(params);
        } else if (input.substr(0,18) == "deribit_order_book") {
            std::string cmd;
            trade_handler::order_book_params params;
            params.depth = -1;
            
            std::stringstream ss{input};
            ss >> cmd >> params.instrument >> params.depth;

            trader.get_order_book(params);

        } else if (input.substr(0,11) == "deribit_sub") {
            std::string cmd;
            std::string channels_str;
            trade_handler::subscriptions_params params;

            std::stringstream ss{input};

            ss >> cmd;
            std::getline(ss, channels_str);

            std::stringstream ss2{channels_str};
            std::istream_iterator<std::string> begin(ss2);
            std::istream_iterator<std::string> end;
            // std::vector<std::string> vstrings(begin, end);
            params.channels = std::vector<std::string>(begin, end);

            trader.subscribe(params);

        } else if (input.substr(0,13) == "deribit_unsub") {
            trader.unsubscribe_all();

        } else if (input.substr(0,12) == "deribit_show") {
            trader.print_trade_messages();

        } else if (input.substr(0,12) == "deribit_test") {
            trader.test_trade_api();

        } else if (input.substr(0,14) == "deribit_logout") {
            std::string cmd;
            trade_handler::logout_params params;

            std::stringstream ss(input);
            ss >> cmd >> params.invalidate_token;

            trader.logout(params);

        } else {
            std::cout << "Unrecognized Command" << std::endl;
        }
    }

    return 0;
}