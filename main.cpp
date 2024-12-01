#include <iostream>
#include <string>
#include <vector>
#include "TastyworksSession.hpp"
#include "MarketDataWebSocket.hpp"

int main() {
    try {
        // Initialize session
        TastyworksSession session;
        session.authenticate();

        // Get the WebSocket token
        auto tokenData = session.getQuoteToken();

        // Correctly access the nested "token" field inside "data"
        auto data = tokenData.at("data").as_object();
        std::string websocketToken = data.at("token").as_string().c_str();

        // Debug print token
        std::cout << "WebSocket token: " << websocketToken << std::endl;

        // Define the symbols to retrieve prices for
        std::vector<std::string> symbols = {"/6BZ24:XCME"};

        // Initialize and connect to WebSocket
        MarketDataWebSocket wsClient(
            "wss://tasty-openapi-ws.dxfeed.com/realtime",
            websocketToken, // Pass the correctly parsed WebSocket token
            3               // Channel number
        );

        // Set symbols to track
        wsClient.setSymbolsToTrack(symbols);

        // Connect to the WebSocket server
        wsClient.connect();

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
