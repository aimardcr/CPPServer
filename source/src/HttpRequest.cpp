#include <vector>
#include <sstream>

#include "Defs.h"
#include "HttpRequest.h"
#include "Utils.h"

HttpRequest::HttpRequest(socket_t fd) : connfd(fd) {}

bool HttpRequest::readRequest() {
    if (!setTimeout()) return false;
    return readHttpRequest();
}

bool HttpRequest::parseHeaders(const std::string& headerData) {
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

void HttpRequest::parseQueryParams() {
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

void HttpRequest::parseFormData() {
    if (!headers.has("Content-Type")) {
        return;
    }

    const auto& contentType = headers["Content-Type"];
    if (contentType.find("multipart/form-data") == 0) {
        parseMultipartData();
    } else if (contentType == "application/x-www-form-urlencoded" && !body.empty()) {
        auto parsed = Utils::parseUrlEncoded(body);
        for (const auto& [key, value] : parsed) {
            forms[key] = value;
        }
    }
}

std::string HttpRequest::getBoundary() const {
    const auto& contentType = headers["Content-Type"];
    size_t boundaryPos = contentType.find("boundary=");
    if (boundaryPos == std::string::npos) {
        return "";
    }
    
    return contentType.substr(boundaryPos + 9);
}

std::vector<HttpRequest::MultipartPart> HttpRequest::splitMultipartData() const {
    std::vector<MultipartPart> parts;
    std::string boundary = getBoundary();
    if (boundary.empty() || body.empty()) {
        return parts;
    }

    std::string boundaryStart = "--" + boundary + "\r\n";
    std::string boundaryMiddle = "\r\n--" + boundary + "\r\n";
    std::string boundaryEnd = "\r\n--" + boundary + "--\r\n";

    size_t pos = body.find(boundaryStart);
    if (pos != 0) {
        return parts;
    }

    pos += boundaryStart.length();
    while (pos < body.length()) {
        MultipartPart part;
        
        size_t headersEnd = body.find("\r\n\r\n", pos);
        if (headersEnd == std::string::npos) break;

        std::istringstream headerStream(body.substr(pos, headersEnd - pos));
        std::string headerLine;
        while (std::getline(headerStream, headerLine) && !headerLine.empty()) {
            if (headerLine.back() == '\r') headerLine.pop_back();
            
            size_t colonPos = headerLine.find(':');
            if (colonPos != std::string::npos) {
                std::string key = Utils::trim(headerLine.substr(0, colonPos));
                std::string value = Utils::trim(headerLine.substr(colonPos + 1));
                part.headers[key] = value;
            }
        }

        pos = headersEnd + 4;
        size_t nextBoundary = body.find("\r\n--" + boundary, pos);
        if (nextBoundary == std::string::npos) break;

        part.data = std::vector<char>(
            body.begin() + pos,
            body.begin() + nextBoundary
        );

        parts.push_back(std::move(part));

        pos = nextBoundary + 2 + boundary.length() + 2;
        if (body.substr(nextBoundary, boundaryEnd.length()) == boundaryEnd) {
            break;
        }
    }

    return parts;
}

SafeMap<std::string> HttpRequest::parseContentDisposition(const std::string& header) const {
    SafeMap<std::string> result;
    std::istringstream stream(header);
    std::string item;
    
    stream >> item;
    if (item == "Content-Disposition:") {
        stream >> result["disposition"];
    }
    
    while (stream >> item) {
        if (item.back() == ';') {
            item.pop_back();
        }
        
        size_t equalPos = item.find('=');
        if (equalPos != std::string::npos) {
            std::string key = item.substr(0, equalPos);
            std::string value = item.substr(equalPos + 1);
            
            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            result[key] = value;
        }
    }
    
    return result;
}

void HttpRequest::processMultipartPart(const MultipartPart& part) {
    if (!part.headers.has("Content-Disposition")) {
        return;
    }

    auto disposition = parseContentDisposition(part.headers["Content-Disposition"]);
    if (!disposition.has("name")) {
        return;
    }

    std::string name = disposition["name"];

    if (disposition.has("filename")) {
        std::string filename = disposition["filename"];
        std::string contentType = part.headers.get("Content-Type", "application/octet-stream");
        
        files[name] = UploadedFile(
            name,
            filename,
            contentType,
            std::vector<char>(part.data)
        );
    } else {
        forms[name] = std::string(part.data.begin(), part.data.end());
    }
}

void HttpRequest::parseMultipartData() {
    auto parts = splitMultipartData();
    for (const auto& part : parts) {
        processMultipartPart(part);
    }
}

void HttpRequest::parseJsonData() {
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

void HttpRequest::parseCookies() {
    if (headers.has("Cookie")) {
        auto parsed = Utils::parseUrlEncoded(headers["Cookie"]);
        for (const auto& [key, value] : parsed) {
            cookies[key] = value;
        }
    }
}

bool HttpRequest::readHttpRequest() {
    std::string request;
    std::vector<char> buffer(Config::BUFFER_SIZE);
    size_t contentLength = 0;
    bool headersComplete = false;
    
    while (request.length() < Config::MAX_REQUEST_SIZE) {
        #ifdef _WIN32
            int n = recv(connfd, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (n == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAEINTR) continue;
                return false;
            }
        #else
            ssize_t n = recv(connfd, buffer.data(), buffer.size(), 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                return false;
            }
        #endif
        if (n == 0) return false;  // Connection closed by peer
        
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

bool HttpRequest::setTimeout() {
    #ifdef _WIN32
        DWORD timeout = Config::SOCKET_TIMEOUT * 1000;  // milliseconds
        return setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, 
                         reinterpret_cast<const char*>(&timeout), 
                         sizeof(timeout)) != SOCKET_ERROR;
    #else
        struct timeval tv;
        tv.tv_sec = Config::SOCKET_TIMEOUT;
        tv.tv_usec = 0;
        return setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
    #endif
}