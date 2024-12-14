#include "dxFeedSession.hpp"
#include <iostream>  // Include for std::cout and std::endl
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>

namespace http = boost::beast::http;
namespace json = boost::json;

// Constants
const std::string SESSION_URL = "https://api.tastyworks.com/sessions";
const std::string QUOTE_TOKEN_URL = "https://api.tastyworks.com/api-quote-tokens";

boost::json::value dxFeedSession::loadConfig() {
    std::string configPath =
#ifdef __APPLE__
        "/Users/michaelkilchenmann/Library/Mobile Documents/com~apple~CloudDocs/C_Code/J_Workbench/TT-px-snapshot-latency/creds.json";
#else
        "/home/ec2-user/tt/creds.json";
#endif

    std::ifstream file(configPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file at: " + configPath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    // Debugging: Output the raw file content
    // std::cout << "Raw configuration content:\n" << buffer.str() << std::endl;

    std::string fileContent = buffer.str();

    // Check for empty file
    if (fileContent.empty()) {
        throw std::runtime_error("Configuration file is empty.");
    }

    // Parse the content
    try {
        return json::parse(fileContent);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse configuration: " + std::string(e.what()));
    }
}


dxFeedSession::dxFeedSession() {
    auto config = loadConfig();
    auto userArray = config.at("user").as_array();
    auto passwordArray = config.at("pw").as_array();

    user = json::value_to<std::string>(userArray[0]);
    password = json::value_to<std::string>(passwordArray[0]);
}

void dxFeedSession::authenticate() {
    try {
        boost::asio::io_context ioc;
        boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tlsv12_client);
        ssl_context.set_default_verify_paths();
        ssl_context.set_verify_mode(boost::asio::ssl::verify_peer);

        boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ssl_context);
        boost::asio::ip::tcp::resolver resolver(ioc);
        auto const results = resolver.resolve("api.tastyworks.com", "443");
        boost::beast::get_lowest_layer(stream).connect(results);
        stream.handshake(boost::asio::ssl::stream_base::client);

        boost::json::object payload{
            {"login", user},
            {"password", password},
            {"remember-me", true}
        };
        std::string body = boost::json::serialize(payload);

        boost::beast::http::request<boost::beast::http::string_body> req(
            boost::beast::http::verb::post,
            "/sessions",
            11); // HTTP 1.1
        req.set(boost::beast::http::field::host, "api.tastyworks.com");
        req.set(boost::beast::http::field::content_type, "application/json");
        req.set(boost::beast::http::field::connection, "keep-alive");
        req.set(boost::beast::http::field::user_agent, "curl/7.68.0");
        req.set(boost::beast::http::field::accept, "*/*");
        req.body() = body;
        req.prepare_payload();

        // std::cout << "Request:\n" << req << std::endl;

        boost::beast::http::write(stream, req);

        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res);

        // std::cout << "Response:\n" << res << std::endl;

        if (res.result() == boost::beast::http::status::created) { // HTTP 201 Created
            auto jsonResponse = boost::json::parse(res.body()).as_object();
            auto data = jsonResponse["data"].as_object();
            sessionToken = data["session-token"].as_string().c_str();
            // std::cout << "Session token obtained: " << sessionToken << std::endl;
        } else {
            throw std::runtime_error("Failed to authenticate: " + res.body());
        }

        // Graceful shutdown with error handling
        try {
            stream.shutdown();
        } catch (const boost::system::system_error& e) {
            if (e.code() != boost::asio::error::eof && e.code() != boost::asio::ssl::error::stream_truncated) {
                throw; // Rethrow unexpected errors
            }
            std::cerr << "SSL Shutdown Warning: " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        throw;
    }
}

boost::json::value dxFeedSession::getQuoteToken() {
    try {
        boost::asio::io_context ioc;
        boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tlsv12_client);
        ssl_context.set_default_verify_paths();

        boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ssl_context);
        boost::asio::ip::tcp::resolver resolver(ioc);
        auto const results = resolver.resolve("api.tastyworks.com", "443");
        boost::beast::get_lowest_layer(stream).connect(results);
        stream.handshake(boost::asio::ssl::stream_base::client);

        // Prepare the HTTP request
        boost::beast::http::request<boost::beast::http::string_body> req(
            boost::beast::http::verb::get,
            "/api-quote-tokens",
            11); // HTTP 1.1
        req.set(boost::beast::http::field::host, "api.tastyworks.com");
        req.set(boost::beast::http::field::authorization, sessionToken); // Add Authorization header
        req.set(boost::beast::http::field::connection, "keep-alive");
        req.set(boost::beast::http::field::user_agent, "curl/7.68.0");
        req.set(boost::beast::http::field::accept, "*/*");

        // std::cout << "Request:\n" << req << std::endl;

        // Send the request
        boost::beast::http::write(stream, req);

        // Receive the response
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res);

        // std::cout << "Response:\n" << res << std::endl;

        // Check response status
        if (res.result() != boost::beast::http::status::ok) {
            throw std::runtime_error("Failed to retrieve quote token: " + res.body());
        }

        // Parse the response JSON
        auto jsonResponse = boost::json::parse(res.body()).as_object();
        if (jsonResponse.contains("data")) {
            auto data = jsonResponse["data"].as_object();
            if (data.contains("token")) {
                websocketToken = data["token"].as_string().c_str(); // Extract WebSocket token
                // std::cout << "WebSocket token: " << websocketToken << std::endl;
            } else {
                throw std::runtime_error("Error: 'token' not found in 'data'.");
            }
        } else {
            throw std::runtime_error("Error: 'data' not found in JSON response.");
        }

        return jsonResponse;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        throw;
    }
}


void dxFeedSession::closeSession() {
    boost::asio::io_context ioc;
    boost::asio::ssl::context sslContext(boost::asio::ssl::context::tlsv12_client);
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, sslContext);

    boost::asio::ip::tcp::resolver resolver(ioc);
    auto const results = resolver.resolve("api.tastyworks.com", "443");

    boost::beast::get_lowest_layer(stream).connect(results);
    stream.handshake(boost::asio::ssl::stream_base::client);

    http::request<http::empty_body> req(http::verb::delete_, "/sessions", 11);
    req.set(http::field::host, "api.tastyworks.com");
    req.set(http::field::authorization, sessionToken);
    req.prepare_payload();

    http::write(stream, req);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    if (res.result() == http::status::ok || res.result() == http::status::no_content) {
        std::cout << "Session closed successfully." << std::endl;
    } else {
        throw std::runtime_error("Failed to close session: " + res.body());
    }
}
