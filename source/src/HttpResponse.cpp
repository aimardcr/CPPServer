#include "HttpResponse.h"
#include "Utils.h"
#include "MimeType.h"
#include <sstream>
#include <filesystem>

extern const MimeType MIME_TYPES[];

HttpResponse::HttpResponse(int fd) : connfd(fd), statusCode(HttpStatus::OK) {}

HttpResponse& HttpResponse::setStatus(HttpStatus code) { 
    statusCode = code; 
    return *this; 
}

HttpResponse& HttpResponse::setStatus(int code) { 
    statusCode = static_cast<HttpStatus>(code); 
    return *this; 
}

HttpResponse& HttpResponse::setHeader(const std::string& key, const std::string& value) { 
    headers[key] = value; 
    return *this; 
}

HttpResponse& HttpResponse::setBody(const std::string& content) { 
    body = content; 
    return *this; 
}

HttpResponse& HttpResponse::setJson(const json& data) { 
    body = data.dump(); 
    setHeader("Content-Type", "application/json"); 
    return *this; 
}

HttpResponse& HttpResponse::setCookie(const std::string& key, const std::string& value, const std::string& path = "/", int maxAge = 0, bool secure = false, bool httpOnly = false) {
    std::ostringstream cookie;
    cookie << key << "=" << value;
    
    if (!path.empty()) {
        cookie << "; Path=" << path;
    }
    
    if (maxAge > 0) {
        cookie << "; Max-Age=" << maxAge;
    }
    
    if (secure) {
        cookie << "; Secure";
    }
    
    if (httpOnly) {
        cookie << "; HttpOnly";
    }

    bool cookie_found = false;
    std::vector<std::string> new_cookies;
    
    auto range = headers.equal_range("Set-Cookie");
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.substr(0, key.length()) == key && 
            it->second[key.length()] == '=') {
            new_cookies.push_back(cookie.str());
            cookie_found = true;
        } else {
            new_cookies.push_back(it->second);
        }
    }
    
    if (!cookie_found) {
        new_cookies.push_back(cookie.str());
    }
    
    headers.erase("Set-Cookie");
    
    for (const auto& c : new_cookies) {
        headers.insert({"Set-Cookie", c});
    }
    
    return *this;
}

HttpResponse& HttpResponse::redirect(const std::string& location, HttpStatus status) {
    setHeader("Location", location);
    return setStatus(status);
}

std::pair<HttpStatus, std::string> HttpResponse::sendFile(const std::string& filePath) {
    const std::string fullPath = std::string(Config::STATIC_DIR) + "/" + filePath;
    if (!std::filesystem::exists(fullPath)) {
        return std::make_pair(HttpStatus::NOT_FOUND, "Not Found\n");
    }

    try {
        std::string content = Utils::readFile(fullPath);
        setBody(content);

        std::string contentType = "application/octet-stream";
        for (const auto& signature : MIME_TYPES) {
            if (checkMimeType(reinterpret_cast<const unsigned char*>(content.c_str()), 
                            content.size(), 
                            signature)) {
                contentType = signature.mimeType;
                break;
            }
        }

        setHeader("Content-Type", contentType);
        return std::make_pair(HttpStatus::OK, "");
    } catch (const std::exception& e) {
        return std::make_pair(HttpStatus::INTERNAL_SERVER_ERROR, std::string(e.what()) + "\n");
    }
}

std::string HttpResponse::renderTemplate(const std::string& templateName) {
    std::string templatePath = std::string(Config::TEMPLATE_DIR) + "/" + templateName;
    if (!std::filesystem::exists(templatePath)) {
        throw std::runtime_error("Template not found: " + templatePath);
    }

    try {
        body = Utils::readFile(templatePath);
        headers["Content-Type"] = "text/html";
        return body;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to render template: " + std::string(e.what()));
    }
}

std::string HttpResponse::toString() const {
    std::ostringstream response;
    response << "HTTP/1.1 " << (int)statusCode << " " << getStatusText() << "\r\n";
    
    auto headersCopy = headers;
    if (headersCopy.find("Content-Length") == headersCopy.end()) {
        headersCopy["Content-Length"] = std::to_string(body.length());
    }
    if (headersCopy.find("Connection") == headersCopy.end()) {
        headersCopy["Connection"] = "close";
    }
    if (headersCopy.find("Server") == headersCopy.end()) {
        headersCopy["Server"] = "CPPServer/1.1";
    }

    for (const auto& [key, value] : headersCopy) {
        response << key << ": " << value << "\r\n";
    }
    response << "\r\n" << body;
    return response.str();
}

std::string HttpResponse::getStatusText() const {
    switch (statusCode) {
        // 2xx Success
        case HttpStatus::OK: return "OK";
        case HttpStatus::CREATED: return "Created";
        case HttpStatus::ACCEPTED: return "Accepted";
        case HttpStatus::NON_AUTHORITATIVE_INFORMATION: return "Non-Authoritative Information";
        case HttpStatus::NO_CONTENT: return "No Content";
        case HttpStatus::RESET_CONTENT: return "Reset Content";
        case HttpStatus::PARTIAL_CONTENT: return "Partial Content";
        case HttpStatus::MULTI_STATUS: return "Multi-Status";
        case HttpStatus::ALREADY_REPORTED: return "Already Reported";
        case HttpStatus::IM_USED: return "IM Used";

        // 3xx Redirection
        case HttpStatus::MULTIPLE_CHOICES: return "Multiple Choices";
        case HttpStatus::MOVED_PERMANENTLY: return "Moved Permanently";
        case HttpStatus::FOUND: return "Found";
        case HttpStatus::SEE_OTHER: return "See Other";
        case HttpStatus::NOT_MODIFIED: return "Not Modified";
        case HttpStatus::USE_PROXY: return "Use Proxy";
        case HttpStatus::TEMPORARY_REDIRECT: return "Temporary Redirect";
        case HttpStatus::PERMANENT_REDIRECT: return "Permanent Redirect";

        // 4xx Client Error
        case HttpStatus::BAD_REQUEST: return "Bad Request";
        case HttpStatus::UNAUTHORIZED: return "Unauthorized";
        case HttpStatus::PAYMENT_REQUIRED: return "Payment Required";
        case HttpStatus::FORBIDDEN: return "Forbidden";
        case HttpStatus::NOT_FOUND: return "Not Found";
        case HttpStatus::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HttpStatus::NOT_ACCEPTABLE: return "Not Acceptable";
        case HttpStatus::PROXY_AUTHENTICATION_REQUIRED: return "Proxy Authentication Required";
        case HttpStatus::REQUEST_TIMEOUT: return "Request Timeout";
        case HttpStatus::CONFLICT: return "Conflict";
        case HttpStatus::GONE: return "Gone";
        case HttpStatus::LENGTH_REQUIRED: return "Length Required";
        case HttpStatus::PRECONDITION_FAILED: return "Precondition Failed";
        case HttpStatus::PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case HttpStatus::URI_TOO_LONG: return "URI Too Long";
        case HttpStatus::UNSUPPORTED_MEDIA_TYPE: return "Unsupported Media Type";
        case HttpStatus::RANGE_NOT_SATISFIABLE: return "Range Not Satisfiable";
        case HttpStatus::EXPECTATION_FAILED: return "Expectation Failed";
        case HttpStatus::IM_A_TEAPOT: return "I'm a teapot";
        case HttpStatus::MISDIRECTED_REQUEST: return "Misdirected Request";
        case HttpStatus::UNPROCESSABLE_ENTITY: return "Unprocessable Entity";
        case HttpStatus::LOCKED: return "Locked";
        case HttpStatus::FAILED_DEPENDENCY: return "Failed Dependency";
        case HttpStatus::TOO_EARLY: return "Too Early";
        case HttpStatus::UPGRADE_REQUIRED: return "Upgrade Required";
        case HttpStatus::PRECONDITION_REQUIRED: return "Precondition Required";
        case HttpStatus::TOO_MANY_REQUESTS: return "Too Many Requests";
        case HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE: return "Request Header Fields Too Large";
        case HttpStatus::UNAVAILABLE_FOR_LEGAL_REASONS: return "Unavailable For Legal Reasons";

        // 5xx Server Error
        case HttpStatus::INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HttpStatus::NOT_IMPLEMENTED: return "Not Implemented";
        case HttpStatus::BAD_GATEWAY: return "Bad Gateway";
        case HttpStatus::SERVICE_UNAVAILABLE: return "Service Unavailable";
        case HttpStatus::GATEWAY_TIMEOUT: return "Gateway Timeout";
        case HttpStatus::HTTP_VERSION_NOT_SUPPORTED: return "HTTP Version Not Supported";
        case HttpStatus::VARIANT_ALSO_NEGOTIATES: return "Variant Also Negotiates";
        case HttpStatus::INSUFFICIENT_STORAGE: return "Insufficient Storage";
        case HttpStatus::LOOP_DETECTED: return "Loop Detected";
        case HttpStatus::NOT_EXTENDED: return "Not Extended";
        case HttpStatus::NETWORK_AUTHENTICATION_REQUIRED: return "Network Authentication Required";
    }
    return "Unknown";
}