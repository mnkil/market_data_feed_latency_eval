#ifndef MARKETDATAWEBSOCKET_HPP
#define MARKETDATAWEBSOCKET_HPP

#include <websocketpp/config/core_client.hpp>
#include <websocketpp/transport/asio/endpoint.hpp>
#include <websocketpp/transport/asio/security/tls.hpp>
#include <websocketpp/client.hpp>
#include <boost/asio/ssl/context.hpp>
#include <memory>
#include <string>
#include <vector>
#include <map> // Include map for symbol tracking

// Custom TLS configuration for WebSocket++ client
struct custom_tls_config : public websocketpp::config::core_client {
    typedef custom_tls_config type;
    typedef core_client base;

    typedef base::concurrency_type concurrency_type;
    typedef base::request_type request_type;
    typedef base::response_type response_type;
    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;
    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;
    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config {
        typedef type::concurrency_type concurrency_type;
        typedef type::alog_type alog_type;
        typedef type::elog_type elog_type;
        typedef type::request_type request_type;
        typedef type::response_type response_type;
        typedef websocketpp::transport::asio::tls_socket::endpoint socket_type;
    };

    typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

    // TLS context initialization
    static std::shared_ptr<boost::asio::ssl::context> on_tls_init(websocketpp::connection_hdl) {
        auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
        ctx->set_default_verify_paths(); // Use system's default CA certificates
        ctx->set_verify_mode(boost::asio::ssl::verify_peer); // Verify peers
        return ctx;
    }
};

// WebSocket client typedef
typedef websocketpp::client<custom_tls_config> WebSocketClient;

class MarketDataWebSocket {
private:
    std::string wsUrl;
    std::string token;
    int channelNumber;
    std::vector<std::string> symbolsToTrack;

    // Add a map to track symbol statuses
    std::map<std::string, bool> symbolsStatus;

    WebSocketClient client;

public:
    // Constructor
    MarketDataWebSocket(const std::string& url, const std::string& authToken, int channel);

    // Connect to the WebSocket server
    void connect();

    // Handlers
    void onOpen(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, const std::string& message);

    // Set symbols to track
    void setSymbolsToTrack(const std::vector<std::string>& symbols);
};

#endif // MARKETDATAWEBSOCKET_HPP
