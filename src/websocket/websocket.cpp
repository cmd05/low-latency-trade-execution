#include <websocket/websocket.h>
#include <lib/utilities.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

/// connection_metadata

connection_metadata::connection_metadata(con_id_type id, websocketpp::connection_hdl hdl, std::string uri)
    : m_id(id)
    , m_hdl(hdl)
    , m_status(WS_INIT_STATUS)
    , m_uri(uri)
    , m_server("N/A") {}

void connection_metadata::on_open(client * c, websocketpp::connection_hdl hdl) {
    m_status = WS_OPEN_STATUS;

    client::connection_ptr con = c->get_con_from_hdl(hdl);
    m_server = con->get_response_header("Server");
}

void connection_metadata::on_fail(client * c, websocketpp::connection_hdl hdl) {
    m_status = WS_FAIL_STATUS;

    client::connection_ptr con = c->get_con_from_hdl(hdl);
    m_server = con->get_response_header("Server");
    m_error_reason = con->get_ec().message();
}

void connection_metadata::on_close(client * c, websocketpp::connection_hdl hdl) {
    m_status = WS_CLOSE_STATUS;

    client::connection_ptr con = c->get_con_from_hdl(hdl);
    std::stringstream s;

    s << "close code: " << con->get_remote_close_code() << " (" 
        << websocketpp::close::status::get_string(con->get_remote_close_code()) 
        << "), close reason: " << con->get_remote_close_reason();
    m_error_reason = s.str();
}

void connection_metadata::on_message(client * c, websocketpp::connection_hdl hdl, message_ptr msg) {
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
        m_messages.push_back("RECV: " + msg->get_payload());
    } else {
        m_messages.push_back("RECV: " + websocketpp::utility::to_hex(msg->get_payload()));
    }
}

std::string& connection_metadata::record_sent_message(std::string message) {
    m_messages.push_back("SENT: " + message);
    return m_messages.back();
}

std::ostream & operator<<(std::ostream & out, connection_metadata const & data) {
    out << "> URI: " << data.m_uri << "\n"
        << "> Status: " << data.m_status << "\n"
        << "> Remote Server: " << (data.m_server.empty() ? "None Specified" : data.m_server) << "\n"
        << "> Error/close reason: " << (data.m_error_reason.empty() ? "N/A" : data.m_error_reason) << "\n";
    out << "> Messages Processed: (" << data.m_messages.size() << ") \n\n";

    for (auto it = data.m_messages.begin(); it != data.m_messages.end(); ++it) {
        out << it->substr(0, WS_MSG_TYPE_LEN); // output message type
        auto msg = it->substr(WS_MSG_TYPE_LEN);
        // out << msg << '\n';

        // disable prettifying for profiling
        try {
            json j = json::parse(msg);
            out << std::setw(WS_JSON_FORMAT_WIDTH) << j << '\n';
        } catch(...) {
            out << msg << '\n';
        }
    }

    return out;
}

/// websocket_endpoint

websocket_endpoint::websocket_endpoint(): m_next_id(0) {
    m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
    m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

    m_endpoint.init_asio();
    m_endpoint.start_perpetual(); // run in perpetual mode

    // run endpoint on seperate thread
    m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&client::run, &m_endpoint);
}

websocket_endpoint::~websocket_endpoint() {
    m_endpoint.stop_perpetual(); // stop perpetual mode
    
    for (con_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it) {
        // Only close open connections
        if (it->second->get_status() != WS_OPEN_STATUS)
            continue;

        APP_LOG(log_flags::ws, "> Closing connection " << it->second->get_id());

        websocketpp::lib::error_code ec;
        m_endpoint.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
        
        if (ec)
            APP_LOG(log_flags::ws, "> Error closing connection " << it->second->get_id() << ": " << ec.message());
    }
    
    // wait till thread is complete
    m_thread->join();
}

context_ptr websocket_endpoint::on_tls_init() {
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use);
    } catch (std::exception& e) {
        APP_LOG(log_flags::ws, e.what());
    }

    return ctx;
}

con_id_type websocket_endpoint::connect(const std::string& uri) {
    // use tls connection
    m_endpoint.set_tls_init_handler(websocketpp::lib::bind(&on_tls_init));

    websocketpp::lib::error_code ec;
    con_id_type new_id = m_next_id++;

    client::connection_ptr con = m_endpoint.get_connection(uri, ec);

    if (ec) {
        APP_LOG(log_flags::ws, "> Connect initialization error: " << ec.message());
        return WS_CON_ERR_CODE;
    }

    connection_metadata::ptr metadata_ptr = websocketpp::lib::make_shared<connection_metadata>(new_id, con->get_handle(), uri);
    m_connection_list[new_id] = metadata_ptr; // store the connection and associated metadata

    // register callbacks
    con->set_open_handler(websocketpp::lib::bind(
        &connection_metadata::on_open,
        metadata_ptr,
        &m_endpoint,
        websocketpp::lib::placeholders::_1
    ));

    con->set_fail_handler(websocketpp::lib::bind(
        &connection_metadata::on_fail,
        metadata_ptr,
        &m_endpoint,
        websocketpp::lib::placeholders::_1
    ));

    con->set_close_handler(websocketpp::lib::bind(
        &connection_metadata::on_close,
        metadata_ptr,
        &m_endpoint,
        websocketpp::lib::placeholders::_1
    ));

    con->set_message_handler(websocketpp::lib::bind(
        &connection_metadata::on_message,
        metadata_ptr,
        &m_endpoint,
        websocketpp::lib::placeholders::_1,
        websocketpp::lib::placeholders::_2
    ));

    // intialize connection
    m_endpoint.connect(con);

    return new_id;
}

void websocket_endpoint::close(con_id_type id, websocketpp::close::status::value code, std::string reason) {
    websocketpp::lib::error_code ec;
    
    con_list::iterator metadata_it = m_connection_list.find(id);

    if (metadata_it == m_connection_list.end()) {
        APP_LOG(log_flags::ws, "> No connection found with id " << id);
        return;
    }

    m_endpoint.close(metadata_it->second->get_hdl(), code, reason, ec);
    
    if (ec)
       APP_LOG(log_flags::ws, "> Error initiating close: " << ec.message());
}

websocket_endpoint::send_result websocket_endpoint::send(con_id_type id, std::string message) {
    websocketpp::lib::error_code ec;

    // benchmark send request
    benchmark send_benchmark {"send_request_benchmark"};
    send_benchmark.start();

    con_list::iterator metadata_it = m_connection_list.find(id);
    if (metadata_it == m_connection_list.end()) {
        APP_LOG(log_flags::ws, "> No connection found with id " << id);
        return send_result{ec, "No connection found with id"};
    }
    
    // send message
    m_endpoint.send(metadata_it->second->get_hdl(), message, websocketpp::frame::opcode::text, ec);

    if (ec) {
        APP_LOG(log_flags::ws, "> Error sending message: " << ec.message());
        return send_result{ec, ec.message()};
    }

    // end send request benchmark
    send_benchmark.end();
    // end global benchmark
    g_benchmark.end();

    metadata_it->second->record_sent_message(message);

    return send_result{};
}

connection_metadata::ptr websocket_endpoint::get_metadata(con_id_type id) const {
    con_list::const_iterator metadata_it = m_connection_list.find(id);

    if (metadata_it == m_connection_list.end())
        return connection_metadata::ptr();
    else
        return metadata_it->second;
}

std::string* websocket_endpoint::get_latest_message(con_id_type id) {
    con_list::const_iterator metadata_it = m_connection_list.find(id);

    if (metadata_it == m_connection_list.end() || metadata_it->second->m_messages.size() == 0)
        return nullptr;
    
    return &(metadata_it->second->m_messages.back());
}