#include "HttpServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>

#include "json.hpp"
using json = nlohmann::json;

HttpServer::HttpServer(const std::string& host = "0.0.0.0", int port = 8000) 
    : host_(host), port_(port), sockfd_(-1), running_(false) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::isRunning() const { 
    return running_; 
}

std::string HttpServer::getHost() const { 
    return host_; 
}

void HttpServer::setHost(const std::string& host) {
    if (running_) throw ServerException("Cannot change host while server is running");
    host_ = host;
}

int HttpServer::getPort() const { 
    return port_; 
}

void HttpServer::setPort(int port) {
    if (running_) throw ServerException("Cannot change port while server is running");
    if (port <= 0 || port > 65535) {
        throw ServerException("Port must be between 1 and 65535");
    }
    port_ = port;
}

void HttpServer::run() {
    if (running_) throw ServerException("Server is already running");
    
    setupServer();
    running_ = true;

    std::cout << "Listening on " << host_ << ":" << port_ << std::endl;

    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int connfd = accept(sockfd_, (struct sockaddr*)&client_addr, &client_addr_len);
        if (connfd == -1) {
            if (errno == EINTR) continue;
            if (running_) {
                perror("accept");
            }
            break;
        }

        std::thread([this, connfd]() {
            handleRequest(connfd);
            close(connfd);
        }).detach();
    }

    cleanup();
}

void HttpServer::stop() {
    if (running_) {
        running_ = false;
        if (sockfd_ != -1) {
            close(sockfd_);
            sockfd_ = -1;
        }
    }
}

void HttpServer::handleRequest(int connfd) {
    HttpContext ctx(connfd);

    try {
        if (!ctx.req.readRequest()) {
            ctx.res.setStatus(HttpStatus::BAD_REQUEST);
            ctx.res.setBody("Bad Request\n");
            sendResponse(ctx.res);
            return;
        }

        const auto& method = ctx.req.method;
        const auto& path = ctx.req.path;

        std::cout << method << " " << path << " " << ctx.req.version << std::endl;

        if (path.length() > 1024) {
            ctx.res.setStatus(HttpStatus::URI_TOO_LONG);
            ctx.res.setBody("URI Too Long\n");
            sendResponse(ctx.res);
            return;
        }

        if (method == "GET" && path.find("/" + Config::STATIC_DIR + "/") == 0) {
            std::string file_path = path.substr(Config::STATIC_DIR.length() + 2);
            auto [status, body] = ctx.res.sendFile(file_path);
            ctx.res.setStatus(status);
            if (!body.empty()) {
                ctx.res.setBody(body);
            }
            sendResponse(ctx.res);
            return;
        }

        auto method_it = routes_.find(method);
        if (method_it == routes_.end()) {
            ctx.res.setStatus(HttpStatus::METHOD_NOT_ALLOWED);
            ctx.res.setBody("Method Not Allowed\n");
            sendResponse(ctx.res);
            return;
        }

        auto route_it = method_it->second.find(path);
        if (route_it == method_it->second.end()) {
            ctx.res.setStatus(HttpStatus::NOT_FOUND);
            ctx.res.setBody("Not Found\n");
            sendResponse(ctx.res);
            return;
        }

        try {
            route_it->second(ctx);
        } catch (const std::exception& e) {
            ctx.res.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
            ctx.res.setBody(std::string(e.what()) + "\n");
        }

        sendResponse(ctx.res);

    } catch (const std::exception& e) {
        try {
            HttpResponse error_response(connfd);
            error_response.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
            error_response.setBody("Server error: " + std::string(e.what()) + "\n");
            sendResponse(error_response);
        } catch (const std::exception& e2) {
            std::cerr << "Failed to send error response: " << e2.what() << std::endl;
        }
    }
}

void HttpServer::sendResponse(const HttpResponse& response) {
    std::string response_str = response.toString();
    ssize_t total_sent = 0;
    const char* data = response_str.c_str();
    size_t remaining = response_str.length();

    while (total_sent < static_cast<ssize_t>(response_str.length())) {
        ssize_t sent = send(response.getConnfd(), 
                            data + total_sent, 
                            remaining, 
                            0);
        
        if (sent == -1) {
            if (errno == EINTR) continue;
            throw ServerException("Failed to send response: " + std::string(strerror(errno)));
        }

        total_sent += sent;
        remaining -= sent;
    }
}

void HttpServer::setupServer() {
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ == -1) {
        throw ServerException("Failed to create socket: " + std::string(strerror(errno)));
    }

    int yes = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        close(sockfd_);
        throw ServerException("Failed to set socket options: " + std::string(strerror(errno)));
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    
    if (host_ == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
    }

    if (bind(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(sockfd_);
        throw ServerException("Failed to bind socket: " + std::string(strerror(errno)));
    }

    if (listen(sockfd_, SOMAXCONN) == -1) {
        close(sockfd_);
        throw ServerException("Failed to listen on socket: " + std::string(strerror(errno)));
    }
}

void HttpServer::cleanup() {
    if (sockfd_ != -1) {
        close(sockfd_);
        sockfd_ = -1;
    }
    running_ = false;
}