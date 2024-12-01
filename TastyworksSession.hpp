#ifndef TASTYWORKSSESSION_HPP
#define TASTYWORKSSESSION_HPP

#include <string>
#include <boost/json.hpp>

class TastyworksSession {
private:
    std::string user;
    std::string password;
    std::string sessionToken;

    boost::json::value loadConfig();

public:
    TastyworksSession();
    void authenticate();
    boost::json::value getQuoteToken();
    std::string websocketToken; //
    void closeSession();
};

#endif
