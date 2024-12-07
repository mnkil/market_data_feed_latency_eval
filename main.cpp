#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include "dxFeedSession.hpp"
#include "MarketDataWebSocket.hpp"

int main() {
    try {
        // Start total script execution timer
        auto script_start_time = std::chrono::high_resolution_clock::now();
        
        // Initialize session
        dxFeedSession session;
        session.authenticate();

        // Get the WebSocket token
        auto tokenData = session.getQuoteToken();

        // Correctly access the nested "token" field inside "data"
        auto data = tokenData.at("data").as_object();
        std::string websocketToken = data.at("token").as_string().c_str();

        // Debug print token
        // std::cout << "WebSocket token: " << websocketToken << std::endl;

        // Define the symbols to retrieve prices for
        // std::vector<std::string> symbols = {"/6BZ24:XCME", "/6EZ24:XCME"};
        std::vector<std::string> symbols = {"/6EZ24:XCME"};
        // std::vector<std::string> symbols = {"/6BZ24:XCME"};

        // Initialize and connect to WebSocket
        MarketDataWebSocket wsClient(
            "wss://tasty-openapi-ws.dxfeed.com/realtime",
            websocketToken, // Pass the correctly parsed WebSocket token
            3               // Channel number
        );

        // Set symbols to track
        wsClient.setSymbolsToTrack(symbols);
        // Connect to the WebSocket server

        // Start WebSocket connection timer
        auto ws_start_time = std::chrono::high_resolution_clock::now();
        
        wsClient.connect();

        // End WebSocket connection timer
        auto ws_end_time = std::chrono::high_resolution_clock::now();
        auto ws_duration = std::chrono::duration_cast<std::chrono::milliseconds>(ws_end_time - ws_start_time).count();
        std::cout << "WebSocket px return time: " << ws_duration << " ms" << std::endl;

        // End total script execution timer
        auto script_end_time = std::chrono::high_resolution_clock::now();
        auto script_duration = std::chrono::duration_cast<std::chrono::milliseconds>(script_end_time - script_start_time).count();
        std::cout << "Total script execution time: " << script_duration << " ms" << std::endl;


    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
