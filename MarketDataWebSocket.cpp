#include "MarketDataWebSocket.hpp"
#include <iostream>
#include <boost/json.hpp>

MarketDataWebSocket::MarketDataWebSocket(const std::string& url, const std::string& authToken, int channel)
    : wsUrl(url), token(authToken), channelNumber(channel) {}

void MarketDataWebSocket::connect() {
    try {
        // Initialize the WebSocket client
        std::cout << "Initializing WebSocket client..." << std::endl;
        client.init_asio();

        // Set the TLS initialization handler
        client.set_tls_init_handler([](websocketpp::connection_hdl hdl) {
            std::cout << "Setting TLS initialization handler..." << std::endl;
            return custom_tls_config::on_tls_init(hdl);
        });

        // Set up connection handlers
        client.set_open_handler([this](websocketpp::connection_hdl hdl) {
            std::cout << "Connection open handler triggered." << std::endl;
            this->onOpen(hdl);
        });

        client.set_message_handler([this](websocketpp::connection_hdl hdl, WebSocketClient::message_ptr msg) {
            std::cout << "Message handler triggered. Received payload size: " << msg->get_payload().size() << std::endl;
            this->onMessage(hdl, msg->get_payload());
        });

        client.set_fail_handler([this](websocketpp::connection_hdl) {
            std::cerr << "Connection failed." << std::endl;
        });

        client.set_close_handler([this](websocketpp::connection_hdl) {
            std::cerr << "Connection closed by server." << std::endl;
        });

        // Create WebSocket connection
        std::cout << "Creating WebSocket connection to: " << wsUrl << std::endl;
        websocketpp::lib::error_code ec;
        WebSocketClient::connection_ptr con = client.get_connection(wsUrl, ec);
        if (ec) {
            throw std::runtime_error("Connection initialization error: " + ec.message());
        }

        // Add the Authorization header for the token
        std::cout << "Adding Authorization header with token: " << token << std::endl;
        con->append_header("Authorization", token);

        // Connect and run the client
        std::cout << "Connecting to the WebSocket..." << std::endl;
        client.connect(con);
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "WebSocket connection error: " << e.what() << std::endl;
    }
}

void MarketDataWebSocket::onOpen(websocketpp::connection_hdl hdl) {
    std::cout << "### Connection opened ###" << std::endl;

    // Send SETUP message
    boost::json::object setupMessage{
        {"type", "SETUP"},
        {"channel", 0},
        {"version", "0.1-DXF-JS/0.3.0"},
        {"keepaliveTimeout", 15},
        {"acceptKeepaliveTimeout", 20}
    };

    std::cout << "Sending SETUP message: " << boost::json::serialize(setupMessage) << std::endl;
    client.send(hdl, boost::json::serialize(setupMessage), websocketpp::frame::opcode::text);
}

void MarketDataWebSocket::onMessage(websocketpp::connection_hdl hdl, const std::string& message) {
    std::cout << "Message handler triggered. Received payload size: " << message.size() << std::endl;
    std::cout << "Received raw message: " << message << std::endl;

    try {
        auto data = boost::json::parse(message).as_object();

        // Log all message types for debugging
        if (data.contains("type")) {
            std::cout << "Message type: " << data["type"].as_string() << std::endl;
        }

        // Handle AUTH_STATE
        if (data["type"].as_string() == "AUTH_STATE") {
            std::cout << "AUTH_STATE message received." << std::endl;
            if (data["state"].as_string() == "UNAUTHORIZED") {
                std::cout << "State is UNAUTHORIZED. Sending AUTH message." << std::endl;
                boost::json::object authMessage{
                    {"type", "AUTH"},
                    {"channel", 0},
                    {"token", token}
                };
                std::cout << "Sending AUTH message: " << boost::json::serialize(authMessage) << std::endl;
                client.send(hdl, boost::json::serialize(authMessage), websocketpp::frame::opcode::text);
            } else if (data["state"].as_string() == "AUTHORIZED") {
                std::cout << "State is AUTHORIZED. Sending CHANNEL_REQUEST." << std::endl;
                boost::json::object channelRequest{
                    {"type", "CHANNEL_REQUEST"},
                    {"channel", channelNumber},
                    {"service", "FEED"},
                    {"parameters", boost::json::object{{"contract", "AUTO"}}}
                };
                std::cout << "Sending CHANNEL_REQUEST message: " << boost::json::serialize(channelRequest) << std::endl;
                client.send(hdl, boost::json::serialize(channelRequest), websocketpp::frame::opcode::text);
            }
        }

        // Handle CHANNEL_OPENED
        if (data["type"].as_string() == "CHANNEL_OPENED" && data["channel"].as_int64() == channelNumber) {
            std::cout << "CHANNEL_OPENED message received. Sending FEED_SETUP." << std::endl;
            boost::json::object feedSetupMessage{
                {"type", "FEED_SETUP"},
                {"channel", channelNumber},
                {"acceptAggregationPeriod", 0.1},
                {"acceptDataFormat", "COMPACT"},
                {"acceptEventFields", boost::json::object{
                    {"Quote", {"eventType", "eventSymbol", "bidPrice", "askPrice", "bidSize", "askSize"}}
                }}
            };
            std::cout << "Sending FEED_SETUP message: " << boost::json::serialize(feedSetupMessage) << std::endl;
            client.send(hdl, boost::json::serialize(feedSetupMessage), websocketpp::frame::opcode::text);
        }

        // Handle FEED_CONFIG
        if (data["type"].as_string() == "FEED_CONFIG" && data["channel"].as_int64() == channelNumber) {
            std::cout << "FEED_CONFIG message received. Sending FEED_SUBSCRIPTION." << std::endl;

            boost::json::array symbolsArray;
            for (const auto& symbol : symbolsToTrack) {
                std::cout << "Subscribing to symbol: " << symbol << std::endl;
                symbolsArray.push_back(boost::json::object{{"type", "Quote"}, {"symbol", symbol}});
            }

            boost::json::object subscriptionMessage{
                {"type", "FEED_SUBSCRIPTION"},
                {"channel", channelNumber},
                {"reset", true},
                {"add", symbolsArray}
            };

            std::cout << "Sending FEED_SUBSCRIPTION message: "
                      << boost::json::serialize(subscriptionMessage) << std::endl;

            client.send(hdl, boost::json::serialize(subscriptionMessage), websocketpp::frame::opcode::text);
        }

        // Handle FEED_DATA
        if (data["type"].as_string() == "FEED_DATA" && data["channel"].as_int64() == channelNumber) {
            std::cout << "FEED_DATA message received. Processing market data." << std::endl;

            auto feedData = data["data"].as_array();
            for (const auto& entry : feedData) {
                auto obj = entry.as_object();
                std::string eventType = std::string(obj["eventType"].as_string().c_str());
                std::string eventSymbol = std::string(obj["eventSymbol"].as_string().c_str());
                double bidPrice = obj["bidPrice"].as_double();
                double askPrice = obj["askPrice"].as_double();
                double midPrice = (bidPrice + askPrice) / 2;

                std::cout << "Symbol: " << eventSymbol
                          << " | Bid: " << bidPrice
                          << " | Ask: " << askPrice
                          << " | Mid: " << midPrice << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse message: " << e.what() << "\nRaw Message: " << message << std::endl;
    }
}


void MarketDataWebSocket::setSymbolsToTrack(const std::vector<std::string>& symbols) {
    symbolsToTrack = symbols;
    std::cout << "Symbols to track set: ";
    for (const auto& symbol : symbols) {
        std::cout << symbol << " ";
    }
    std::cout << std::endl;
}
