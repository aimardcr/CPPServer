#include <vector>
#include <sstream>
#include <iostream>

#include "Defs.h"
#include "HttpRequest.h"
#include "Utils.h"

HttpRequest::HttpRequest(socket_t fd) : connfd(fd) {}

bool HttpRequest::setTimeout() {
    #ifdef _WIN32
        DWORD timeout = Config::SOCKET_TIMEOUT * 1000;
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
    bool isChunked = false;
    
    while (request.length() < Config::MAX_REQUEST_SIZE) {
        if (!headersComplete) {
            int n = recv(connfd, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (n <= 0) {
                if (n < 0) {
                    #ifdef _WIN32
                        if (WSAGetLastError() == WSAEINTR) continue;
                    #else
                        if (errno == EINTR) continue;
                    #endif
                }
                return false;
            }
            
            request.append(buffer.data(), n);
            
            size_t headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                if (!parseHeaders(request.substr(0, headerEnd))) {
                    return false;
                }
                headersComplete = true;
                
                if (headers.has("Transfer-Encoding") && 
                    headers["Transfer-Encoding"].find("chunked") != std::string::npos) {
                    isChunked = true;
                } else if (headers.has("Content-Length")) {
                    contentLength = std::stoul(headers["Content-Length"]);
                    if (contentLength > Config::MAX_REQUEST_SIZE) {
                        return false;
                    }
                }
                
                body = request.substr(headerEnd + 4);
                
                if (isChunked) {
                    std::string remaining_data = body;
                    body.clear();
                    
                    size_t pos = 0;
                    while (pos < remaining_data.length()) {
                        size_t chunk_header_end = remaining_data.find("\r\n", pos);
                        if (chunk_header_end == std::string::npos) break;
                        
                        std::string size_str = remaining_data.substr(pos, chunk_header_end - pos);
                        size_t chunk_size;
                        try {
                            chunk_size = std::stoull(size_str, nullptr, 16);
                        } catch (...) {
                            break;
                        }
                        
                        if (remaining_data.length() < chunk_header_end + 2 + chunk_size + 2) {
                            break;
                        }
                        
                        if (chunk_size > 0) {
                            body.append(remaining_data.substr(chunk_header_end + 2, chunk_size));
                        }
                        
                        pos = chunk_header_end + 2 + chunk_size + 2;
                        
                        if (chunk_size == 0) {
                            return true;
                        }
                    }
                    
                    return readRemainingChunks();
                }
                break;
            }
        }
    }

    parseQueryParams();
    parseFormData();
    parseCookies();
    parseJsonData();
    
    return true;
}

bool HttpRequest::readChunk(std::string& chunk, size_t& chunk_size) {
    std::vector<char> buffer(Config::BUFFER_SIZE);
    std::string chunk_header;
    bool header_complete = false;
    
    
    while (!header_complete) {
        int n = recv(connfd, buffer.data(), 1, 0);
        if (n <= 0) {
            return false;
        }
        
        chunk_header += buffer[0];
        if (chunk_header.length() >= 2 && 
            chunk_header.substr(chunk_header.length() - 2) == "\r\n") {
            header_complete = true;
        }
        
        if (chunk_header.length() > 1024) {
            return false;
        }
    }
    
    chunk_header = chunk_header.substr(0, chunk_header.length() - 2);
    
    size_t pos = chunk_header.find(';');
    std::string size_str = (pos != std::string::npos) ? chunk_header.substr(0, pos) : chunk_header;
    
    try {
        chunk_size = std::stoull(size_str, nullptr, 16);
    } catch (const std::exception& e) {
        return false;
    }
    
    if (chunk_size > Config::MAX_REQUEST_SIZE) {
        return false;
    }
    
    if (chunk_size == 0) {
        char crlf[2];
        if (recv(connfd, crlf, 2, 0) != 2 || crlf[0] != '\r' || crlf[1] != '\n') {
            return false;
        }
        return true;
    }
    
    size_t total_read = 0;
    std::vector<char> chunk_buffer(chunk_size);
    
    while (total_read < chunk_size) {
        int to_read = static_cast<int>(chunk_size - total_read);
        int n = recv(connfd, chunk_buffer.data() + total_read, to_read, 0);
        
        if (n <= 0) {
            return false;
        }
        
        total_read += n;
    }
    
    chunk.append(chunk_buffer.data(), chunk_size);
    
    char crlf[2];
    if (recv(connfd, crlf, 2, 0) != 2 || crlf[0] != '\r' || crlf[1] != '\n') {
        return false;
    }
    
    return true;
}

std::string HttpRequest::readLine() {
    std::string line;
    char c;
    
    while (true) {
        int n = recv(connfd, &c, 1, 0);
        if (n <= 0) {
            if (n < 0) {
                #ifdef _WIN32
                    if (WSAGetLastError() == WSAEINTR) continue;
                #else
                    if (errno == EINTR) continue;
                #endif
            }
            break;
        }
        
        line += c;
        if (line.length() >= 2 && line.substr(line.length() - 2) == "\r\n") {
            break;
        }
    }
    
    return line;
}

bool HttpRequest::parseChunkSize(const std::string& line, size_t& size) {
    std::string size_str = line.substr(0, line.find_first_of(" \t\r\n;"));
    try {
        size = std::stoull(size_str, nullptr, 16);
        return true;
    } catch (...) {
        return false;
    }
}


bool HttpRequest::readChunkedBody(std::string& request) {
    std::string chunked_body;
    size_t chunk_size;
    
    while (true) {
        if (!readChunk(chunked_body, chunk_size)) {
            return false;
        }

        if (chunk_size == 0) {
            break;
        }

        if (chunked_body.length() > Config::MAX_REQUEST_SIZE) {
            return false;
        }
    }

    request = chunked_body;
    return true;
}

bool HttpRequest::readRemainingChunks() {
    std::vector<char> buffer(Config::BUFFER_SIZE);
    std::string chunk_size_str;
    size_t total_size = body.length();
    
    while (true) {
        chunk_size_str.clear();
        while (true) {
            int n = recv(connfd, buffer.data(), 1, 0);
            if (n <= 0) return false;
            
            chunk_size_str += buffer[0];
            if (chunk_size_str.length() >= 2 && 
                chunk_size_str.substr(chunk_size_str.length() - 2) == "\r\n") {
                chunk_size_str = chunk_size_str.substr(0, chunk_size_str.length() - 2);
                break;
            }
        }
        
        size_t chunk_size;
        try {
            chunk_size = std::stoull(chunk_size_str, nullptr, 16);
        } catch (...) {
            return false;
        }
        
        if (chunk_size == 0) {
            char crlf[2];
            if (recv(connfd, crlf, 2, 0) != 2) return false;
            return true;
        }
        
        size_t bytes_read = 0;
        while (bytes_read < chunk_size) {

#ifdef _WIN32
            int to_read = static_cast<int>(min(
                chunk_size - bytes_read,
                static_cast<size_t>(Config::BUFFER_SIZE)
            ));
#else
            int to_read = static_cast<int>(std::min(
                chunk_size - bytes_read,
                static_cast<size_t>(Config::BUFFER_SIZE)
            ));
#endif
            
            int n = recv(connfd, buffer.data(), to_read, 0);
            if (n <= 0) return false;
            
            body.append(buffer.data(), n);
            bytes_read += n;
        }
        
        char crlf[2];
        if (recv(connfd, crlf, 2, 0) != 2) return false;
        
        total_size += chunk_size;
        if (total_size > Config::MAX_REQUEST_SIZE) {
            return false;
        }
    }
    
    return true;
}