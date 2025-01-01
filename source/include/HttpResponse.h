#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <map>
#include <utility>

#include "Defs.h"
#include "MimeType.h"
#include "Config.h"
#include "HttpStatus.h"
#include "HttpRequest.h"

#include "json.hpp"
using json = nlohmann::json;

class HttpResponse {
public:
    explicit HttpResponse(socket_t fd, HttpRequest& req);
    ~HttpResponse() = default;

    HttpResponse& setStatus(HttpStatus code);
    HttpResponse& setStatus(int code);
    HttpResponse& setHeader(const std::string& key, const std::string& value);
    HttpResponse& setBody(const std::string& content);
    HttpResponse& setJson(const json& data);
    
    HttpResponse& setCookie(const std::string& key, const std::string& value, const std::string& path = "/", int maxAge = 0, bool secure = false, bool httpOnly = false);
    HttpResponse& redirect(const std::string& location, HttpStatus status = HttpStatus::FOUND);
    HttpResponse& renderTemplate(const std::string& templateName);

    HttpResponse& sendFile(const std::string& fullPath);

    std::string toString();

    socket_t getConnfd() const { return connfd; }

private:
    socket_t connfd;
    HttpRequest req;

    HttpStatus statusCode;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string getStatusText() const;

    bool shouldCompress(const std::string& contentTypes) const;
    std::string compressGzip(const std::string& content) const;
    void prepareResponse();
};

#endif // HTTP_RESPONSE_H