#include "Response.h"
#include "Utils.h"
#include "MimeType.h"
#include <sstream>
#include <filesystem>

extern const MimeType MIME_TYPES[];

Response::Response(int fd) : connfd(fd), statusCode(200) {}

std::string Response::renderTemplate(const std::string& templateName) {
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

std::pair<int, std::string> Response::sendFile(const std::string& filePath) {
    const std::string fullPath = std::string(Config::STATIC_DIR) + "/" + filePath;
    if (!std::filesystem::exists(fullPath)) {
        return {404, "Not Found\n"};
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
        return {200, ""};
    } catch (const std::exception& e) {
        return {500, "Internal Server Error: " + std::string(e.what())};
    }
}

std::string Response::toString() const {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << getStatusText() << "\r\n";
    
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

std::string Response::getStatusText() const {
    switch (statusCode) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}