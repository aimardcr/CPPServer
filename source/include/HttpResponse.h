#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <map>
#include <utility>
#include "MimeType.h"
#include "Config.h"
#include "HttpStatus.h"

#include "json.hpp"
using json = nlohmann::json;

class HttpResponse {
public:
    explicit HttpResponse(int fd);
    ~HttpResponse() = default;

    HttpResponse& setStatus(HttpStatus code);
    HttpResponse& setStatus(int code);
    HttpResponse& setHeader(const std::string& key, const std::string& value);
    HttpResponse& setBody(const std::string& content);
    HttpResponse& setJson(const json& data);
    
    HttpResponse& setCookie(const std::string& key, const std::string& value, const std::string& path = "/", int maxAge = 0, bool secure = false, bool httpOnly = false);
    HttpResponse& redirect(const std::string& location, HttpStatus status = HttpStatus::FOUND);
    HttpResponse& renderTemplate(const std::string& templateName);

    std::pair<HttpStatus, std::string> sendFile(const std::string& filePath);

    std::string toString() const;

    int getConnfd() const { return connfd; }

private:
    int connfd;
    HttpStatus statusCode;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string getStatusText() const;
};

#endif // HTTP_RESPONSE_H