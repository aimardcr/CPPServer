#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>
#include <map>
#include <utility>
#include "MimeType.h"
#include "Config.h"

#include "json.hpp"
using json = nlohmann::json;

class Response {
public:
    explicit Response(int fd);
    ~Response() = default;

    Response& setStatus(int code) { statusCode = code; return *this; }
    Response& setHeader(const std::string& key, const std::string& value) { headers[key] = value; return *this; }
    Response& setBody(const std::string& content) { body = content; return *this; }
    Response& setJson(const json& data) { body = data.dump(); setHeader("Content-Type", "application/json"); return *this; }
    std::pair<int, std::string> sendFile(const std::string& filePath);
    std::string renderTemplate(const std::string& templateName);
    Response& redirect(const std::string& location, int status = 302);

    std::string toString() const;

    int getConnfd() const { return connfd; }

private:
    int connfd;
    int statusCode;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string getStatusText() const;
};

#endif // RESPONSE_H