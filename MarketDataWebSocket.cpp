#include "MarketDataWebSocket.hpp"
#include <iostream>
#include <boost/json.hpp>

MarketDataWebSocket::MarketDataWebSocket(const std::string& url, const std::string& authToken, int channel)
    : wsUrl(url), token(authToken), channelNumber(channel) {}

void MarketDataWebSocket::connect() {
    try {
        // Initialize the WebSocket client
        client.init_asio();

        // Set the TLS initialization handler
        client.set_tls_init_handler([](websocketpp::connection_hdl hdl) {
            return custom_tls_config::on_tls_init(hdl);
        });

        // Set up connection handlers
        client.set_open_handler([this](websocketpp::connection_hdl hdl) {
            this->onOpen(hdl);
        });

        client.set_message_handler([this](websocketpp::connection_hdl hdl, WebSocketClient::message_ptr msg) {
            this->onMessage(hdl, msg->get_payload());
        });

        client.set_fail_handler([this](websocketpp::connection_hdl) {
            std::cerr << "Connection failed." << std::endl;
        });

        client.set_close_handler([this](websocketpp::connection_hdl) {
            std::cerr << "Connection closed." << std::endl;
        });

        // Create WebSocket connection
        websocketpp::lib::error_code ec;
        WebSocketClient::connection_ptr con = client.get_connection(wsUrl, ec);
        if (ec) {
            throw std::runtime_error("Connection initialization error: " + ec.message());
        }

        // Add the Authorization header for the token
        con->append_header("Authorization", token);

        // Connect and run the client
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
    try {
        auto data = boost::json::parse(message).as_object();

        // Handle AUTH_STATE
        if (data["type"].as_string() == "AUTH_STATE") {
            if (data["state"].as_string() == "UNAUTHORIZED") {
                boost::json::object authMessage{
                    {"type", "AUTH"},
                    {"channel", 0},
                    {"token", token}
                };
                std::cout << "Sending AUTH message: " << boost::json::serialize(authMessage) << std::endl;
                client.send(hdl, boost::json::serialize(authMessage), websocketpp::frame::opcode::text);
            } else if (data["state"].as_string() == "AUTHORIZED") {
                boost::json::object channelRequestMessage{
                    {"type", "CHANNEL_REQUEST"},
                    {"channel", channelNumber},
                    {"service", "FEED"},
                    {"parameters", boost::json::object{{"contract", "AUTO"}}}
                };
                std::cout << "Sending CHANNEL_REQUEST message: " << boost::json::serialize(channelRequestMessage) << std::endl;
                client.send(hdl, boost::json::serialize(channelRequestMessage), websocketpp::frame::opcode::text);
            }
        }

        // Handle CHANNEL_OPENED
        if (data["type"].as_string() == "CHANNEL_OPENED" && data["channel"].as_int64() == channelNumber) {
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
            boost::json::array symbolsArray;
            for (const auto& symbol : symbolsToTrack) {
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
            std::string feedType = std::string(feedData[0].as_string().c_str()); // Fix conversion here
            auto marketData = feedData[1].as_array();

            for (size_t i = 0; i < marketData.size(); i += 6) {
                std::string eventType = std::string(marketData[i + 0].as_string().c_str());
                std::string eventSymbol = std::string(marketData[i + 1].as_string().c_str());
                double bidPrice = marketData[i + 2].as_double();
                double askPrice = marketData[i + 3].as_double();
                double midPrice = (bidPrice + askPrice) / 2;
                double bidSize = marketData[i + 4].as_double();
                double askSize = marketData[i + 5].as_double();

                std::cout << "Symbol: " << eventSymbol
                          << " | Bid: " << bidPrice
                          << " | Ask: " << askPrice
                          << " | Mid: " << midPrice
                          << " | bidSize: " << bidSize
                          << " | askSize: " << askSize << std::endl;

                if (symbolsStatus.find(eventSymbol) != symbolsStatus.end()) {
                    symbolsStatus[eventSymbol] = true;
                }
            }

            // Check if all symbols have received data
            if (std::all_of(symbolsStatus.begin(), symbolsStatus.end(),
                            [](const auto& entry) { return entry.second; })) {
                std::cout << "All symbols have received data. Closing WebSocket connection." << std::endl;
                client.stop();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse message: " << e.what() << "\nRaw Message: " << message << std::endl;
    }
}


void MarketDataWebSocket::setSymbolsToTrack(const std::vector<std::string>& symbols) {
    symbolsToTrack = symbols;
    for (const auto& symbol : symbols) {
        symbolsStatus[symbol] = false; // Initialize symbol statuses to false
    }
    std::cout << "Symbols to track set: ";
    for (const auto& symbol : symbols) {
        std::cout << symbol << " ";
    }
    std::cout << std::endl;
}
