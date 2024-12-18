#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>

#include "Defs.h"
#include "SafeMap.h"
#include "Config.h"

#include "json.hpp"

class HttpRequest {
public:
    explicit HttpRequest(socket_t fd);
    ~HttpRequest() = default;

    bool readRequest();

    std::string method;
    std::string path;
    std::string version;

    SafeMap<std::string> headers;
    SafeMap<std::string> params;
    SafeMap<std::string> forms;
    nlohmann::json json;

    SafeMap<std::string> cookies;

private:
    std::string body;
    
    int connfd;
    bool parseHeaders(const std::string& headerData);
    void parseQueryParams();
    void parseFormData();
    void parseJsonData();
    void parseCookies();

    bool readHttpRequest();
    bool setTimeout();
};

#endif // HTTP_REQUEST_H