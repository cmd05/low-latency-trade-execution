#include <client/client_trader.h>

#include <lib/utilities.h>

client_trader::client_trader(trade_handler* trade_handler_, trade_handler::api_key key) {
    m_key = key;
    m_trade_handler = trade_handler_;

    trade_handler_init();
}

void client_trader::test_trade_api() {
    if(!m_trade_api_connected) {
        APP_LOG(log_flags::client_trader, "Not connected to trade API");
        return;
    }

    m_trade_handler->test();
}

void client_trader::buy(trade_handler::order_params params) {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }
    
    m_trade_handler->buy(params);
}

void client_trader::sell(trade_handler::order_params params) {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }

    m_trade_handler->sell(params);
}

void client_trader::edit(trade_handler::order_params params) {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }
    
    m_trade_handler->edit(params);
}

void client_trader::cancel(trade_handler::order_params params) {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }
    
    m_trade_handler->cancel(params);
}

void client_trader::get_open_orders(trade_handler::open_orders_params params) {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }
    
    m_trade_handler->get_open_orders(params);
}

void client_trader::get_order_book(trade_handler::order_book_params params) {
    m_trade_handler->get_order_book(params);
}

void client_trader::print_trade_messages() {
    if(!m_trade_api_connected) {
        APP_LOG(log_flags::client_trader, "Not connected to trade API");
        return;
    }

    connection_metadata::ptr metadata_ptr = m_endpoint.get_metadata(m_trade_api_con_id);

    if(!metadata_ptr)
        APP_LOG(log_flags::client_trader, "Error fetching metadata");
    else
        APP_PRINT(*metadata_ptr);
}

void client_trader::trade_handler_init() {
    m_trade_handler->init(&m_endpoint, m_key);
}

con_id_type client_trader::connect_trade_api() {
    m_trade_api_con_id = m_trade_handler->connect();
    
    if(m_trade_api_con_id != WS_CON_ERR_CODE)
        m_trade_api_connected = true;
    else
        APP_LOG(log_flags::client_trader, "Could not connect to trade API");

    return m_trade_api_con_id;
}

void client_trader::trade_api_auth() {
    if(!m_trade_api_connected) {
        APP_LOG(log_flags::client_trader, "Not connected to trade API");
        return;
    }

    auto ec = m_trade_handler->auth();
    if(!ec)
        m_trade_api_auth = true;
    else
        APP_LOG(log_flags::client_trader, "Authentication failed");
}

void client_trader::get_positions(trade_handler::positions_params params) {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }

    m_trade_handler->get_positions(params);
}

void client_trader::subscribe(trade_handler::subscriptions_params params) {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }

    m_trade_handler->subscribe(params);
}

void client_trader::unsubscribe_all() {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }

    m_trade_handler->unsubscribe_all();
}

void client_trader::logout(trade_handler::logout_params params) {
    if(!m_trade_api_auth) {
        APP_LOG(log_flags::client_trader, "Not authenticated to trade API");
        return;
    }

    m_trade_handler->logout(params);
    m_endpoint.close(m_trade_api_con_id, websocketpp::close::status::going_away, "client logout");

    m_trade_api_auth = false;
    m_trade_api_connected = false;
    m_trade_api_con_id = default_trade_con_id;
}