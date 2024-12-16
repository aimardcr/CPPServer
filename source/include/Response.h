#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>
#include <map>
#include <utility>
#include "MimeType.h"
#include "Config.h"
#include "HttpStatus.h"

#include "json.hpp"
using json = nlohmann::json;

class Response {
public:
    explicit Response(int fd);
    ~Response() = default;

    Response& setStatus(HttpStatus code);
    Response& setStatus(int code);
    Response& setHeader(const std::string& key, const std::string& value);
    Response& setBody(const std::string& content);
    Response& setJson(const json& data);
    
    Response& setCookie(const std::string& key, const std::string& value, const std::string& path, int maxAge, bool secure, bool httpOnly);
    Response& redirect(const std::string& location, HttpStatus status = HttpStatus::FOUND);

    std::pair<HttpStatus, std::string> sendFile(const std::string& filePath);
    std::string renderTemplate(const std::string& templateName);

    std::string toString() const;

    int getConnfd() const { return connfd; }

private:
    int connfd;
    HttpStatus statusCode;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string getStatusText() const;
};

#endif // RESPONSE_H