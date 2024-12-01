#ifndef TASTYWORKS_SESSION_H
#define TASTYWORKS_SESSION_H

#include <string>
#include <map>

class TastyworksSession {
public:
    TastyworksSession(const std::string& config_file);
    void authenticate();
    std::string getQuoteToken();
    void closeSession();
    void run();

private:
    std::string user;
    std::string password;
    std::string session_token;
    std::string quote_token;
    std::string api_base_url;
    std::map<std::string, std::string> loadConfig(const std::string& file_path);
    void sendDiscordMessage(const std::string& message);

    std::string discord_url;
};

#endif
