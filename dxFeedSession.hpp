#ifndef DXFEEDSESSION_HPP
#define DXFEEDSESSION_HPP

#include <string>
#include <boost/json.hpp>
 
class dxFeedSession {
private:
    std::string user;
    std::string password;
    std::string sessionToken;

    boost::json::value loadConfig();

public:
    dxFeedSession();
    void authenticate();
    boost::json::value getQuoteToken();
    std::string websocketToken; //
    void closeSession();
};

#endif
