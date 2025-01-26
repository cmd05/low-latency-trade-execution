#pragma once

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp> 
#include <websocketpp/client.hpp> 

#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include <lib/benchmark.h>
// global benchmark object
extern benchmark g_benchmark;

using con_id_type = int;

#define WS_INIT_STATUS "Connecting"
#define WS_OPEN_STATUS "Open"
#define WS_FAIL_STATUS "Failed"
#define WS_CLOSE_STATUS "Closed"

constexpr int WS_CON_ERR_CODE = -1;
constexpr unsigned int WS_MSG_TYPE_LEN = 6; // 'SENT: ' or 'RECV: '
constexpr unsigned int WS_JSON_FORMAT_WIDTH = 4;

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef client::message_ptr message_ptr;

class connection_metadata {
public:
    typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;

    // constructor
    connection_metadata(con_id_type id, websocketpp::connection_hdl hdl, std::string uri);

    // callback functions
    void on_open(client * c, websocketpp::connection_hdl hdl);
    void on_fail(client * c, websocketpp::connection_hdl hdl);
    void on_close(client * c, websocketpp::connection_hdl hdl);
    void on_message(client * c, websocketpp::connection_hdl hdl, message_ptr msg);

    // modifiers
    std::string& record_sent_message(std::string message);

    // getters / setters
    websocketpp::connection_hdl get_hdl() const { return m_hdl; }
    con_id_type get_id() const { return m_id; }
    std::string get_status() const { return m_status; }

    // operator methods
    friend std::ostream & operator<<(std::ostream & out, connection_metadata const & data);
    friend class websocket_endpoint;
private:
    con_id_type m_id;
    websocketpp::connection_hdl m_hdl;
    std::string m_status;
    std::string m_uri;
    std::string m_server;
    std::string m_error_reason;
    std::vector<std::string> m_messages;
};

class websocket_endpoint {
public:
    struct send_result {
        websocketpp::lib::error_code ec;
        std::string err_message;
    };

    // constructor
    websocket_endpoint();

    // destructor
    ~websocket_endpoint();

    // modifiers
    con_id_type connect(const std::string& uri);
    void close(con_id_type id, websocketpp::close::status::value code, std::string reason);
    send_result send(con_id_type id, std::string message);
    connection_metadata::ptr get_metadata(con_id_type id) const;

    std::string* get_latest_message(con_id_type id);

    // callbacks
    static context_ptr on_tls_init();
private:
    typedef std::map<con_id_type, connection_metadata::ptr> con_list;

    client m_endpoint;
    
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    con_list m_connection_list;
    con_id_type m_next_id;
};