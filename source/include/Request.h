#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include "SafeMap.h"
#include "Config.h"

#include "json.hpp"

class Request {
public:
    explicit Request(int fd);
    ~Request() = default;

    bool readRequest();

    const std::string& getMethod() const { return method; }
    const std::string& getPath() const { return path; }
    const std::string& getVersion() const { return version; }
    const std::string& getBody() const { return body; }

    SafeMap<std::string> headers;
    SafeMap<std::string> params;
    SafeMap<std::string> forms;
    nlohmann::json json;

    SafeMap<std::string> cookies;

private:
    int connfd;
    std::string method;
    std::string path;
    std::string version;
    std::string body;

    bool parseHeaders(const std::string& headerData);
    void parseQueryParams();
    void parseFormData();
    void parseJsonData();
    void parseCookies();

    bool readHttpRequest();
    bool setTimeout();
};

#endif // REQUEST_H