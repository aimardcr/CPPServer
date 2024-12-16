#include "Request.h"
#include "Utils.h"
#include <vector>
#include <sstream>
#include <sys/socket.h>

Request::Request(int fd) : connfd(fd) {}

bool Request::readRequest() {
    if (!setTimeout()) return false;
    return readHttpRequest();
}

bool Request::parseHeaders(const std::string& headerData) {
    std::istringstream stream(headerData);
    std::string line;
    
    if (!std::getline(stream, line) || line.empty()) {
        return false;
    }
    
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream requestLine(line);
    if (!(requestLine >> method >> path >> version)) {
        return false;
    }
    
    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") break;
        
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = Utils::trim(line.substr(0, colon));
            std::string value = Utils::trim(line.substr(colon + 1));
            headers[key] = value;
        }
    }
    
    return true;
}

void Request::parseQueryParams() {
    size_t qPos = path.find('?');
    if (qPos != std::string::npos) {
        std::string query = path.substr(qPos + 1);
        path = path.substr(0, qPos);
        auto parsed = Utils::parseUrlEncoded(query);
        for (const auto& [key, value] : parsed) {
            params[key] = value;
        }
    }
}

void Request::parseFormData() {
    if (headers.has("Content-Type") && 
        headers["Content-Type"] == "application/x-www-form-urlencoded" && 
        !body.empty()) {
        auto parsed = Utils::parseUrlEncoded(body);
        for (const auto& [key, value] : parsed) {
            forms[key] = value;
        }
    }
}

void Request::parseJsonData() {
    if (headers.has("Content-Type") && 
        headers["Content-Type"] == "application/json" && 
        !body.empty()) {
        try {
            json = nlohmann::json::parse(body);
        } catch (const nlohmann::json::exception& e) {
            json = nullptr;
        }
    }
}

void Request::parseCookies() {
    if (headers.has("Cookie")) {
        auto parsed = Utils::parseUrlEncoded(headers["Cookie"]);
        for (const auto& [key, value] : parsed) {
            cookies[key] = value;
        }
    }
}

bool Request::readHttpRequest() {
    std::string request;
    std::vector<char> buffer(Config::BUFFER_SIZE);
    size_t contentLength = 0;
    bool headersComplete = false;
    
    while (request.length() < Config::MAX_REQUEST_SIZE) {
        ssize_t n = recv(connfd, buffer.data(), buffer.size(), 0);
        if (n <= 0) return false;
        
        request.append(buffer.data(), n);
        
        if (!headersComplete) {
            size_t headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                if (!parseHeaders(request.substr(0, headerEnd))) {
                    return false;
                }
                headersComplete = true;
                
                if (headers.has("Content-Length")) {
                    contentLength = std::stoul(headers["Content-Length"]);
                    if (contentLength > Config::MAX_REQUEST_SIZE) {
                        return false;
                    }
                }
            }
        }
        
        if (headersComplete) {
            size_t totalLength = request.find("\r\n\r\n") + 4 + contentLength;
            if (request.length() >= totalLength) {
                body = request.substr(request.find("\r\n\r\n") + 4);
                parseQueryParams();
                parseFormData();
                parseCookies();
                return true;
            }
        }
    }
    return false;
}

bool Request::setTimeout() {
    struct timeval tv;
    tv.tv_sec = Config::SOCKET_TIMEOUT;
    tv.tv_usec = 0;
    return setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}