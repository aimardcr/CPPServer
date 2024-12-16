#include "Server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>

#include "json.hpp"
using json = nlohmann::json;

Server::Server(const std::string& host = "0.0.0.0", int port = 8000) 
    : host_(host), port_(port), sockfd_(-1), running_(false) {
}

Server::~Server() {
    stop();
}

void Server::get(const std::string& path, RouteHandler handler) {
    routes_["GET"][path] = handler;
}

void Server::post(const std::string& path, RouteHandler handler) {
    routes_["POST"][path] = handler;
}

void Server::put(const std::string& path, RouteHandler handler) {
    routes_["PUT"][path] = handler;
}

void Server::patch(const std::string& path, RouteHandler handler) {
    routes_["PATCH"][path] = handler;
}

void Server::delete_(const std::string& path, RouteHandler handler) {
    routes_["DELETE"][path] = handler;
}

void Server::addRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    routes_[method][path] = handler;
}

bool Server::isRunning() const { 
    return running_; 
}

std::string Server::getHost() const { 
    return host_; 
}

void Server::setHost(const std::string& host) {
    if (running_) throw ServerException("Cannot change host while server is running");
    host_ = host;
}

int Server::getPort() const { 
    return port_; 
}

void Server::setPort(int port) {
    if (running_) throw ServerException("Cannot change port while server is running");
    if (port <= 0 || port > 65535) {
        throw ServerException("Port must be between 1 and 65535");
    }
    port_ = port;
}

void Server::run() {
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

void Server::stop() {
    if (running_) {
        running_ = false;
        if (sockfd_ != -1) {
            close(sockfd_);
            sockfd_ = -1;
        }
    }
}

void Server::handleRequest(int connfd) {
    Context ctx(connfd);

    try {
        if (!ctx.req.readRequest()) {
            ctx.res.setStatus(HttpStatus::BAD_REQUEST);
            ctx.res.setBody("Bad Request\n");
        } else {
            const auto& method = ctx.req.getMethod();
            const auto& path = ctx.req.getPath();

            std::cout << method << " " << path << " " << ctx.req.getVersion() << std::endl;

            if (method == "GET" && path.find("/static/") == 0) {
                std::string file_path = path.substr(7);
                auto [status, body] = ctx.res.sendFile(file_path);
                ctx.res.setStatus(status);
                if (!body.empty()) {
                    ctx.res.setBody(body);
                }
            } else {
                auto method_it = routes_.find(method);
                if (method_it != routes_.end()) {
                    auto route_it = method_it->second.find(path);
                    if (route_it != method_it->second.end()) {
                        try {
                            auto response = route_it->second(ctx);

                            if (std::holds_alternative<Response>(response)) {
                                ctx.res = std::get<Response>(response);
                            } else if (std::holds_alternative<std::string>(response)) {
                                ctx.res.setStatus(HttpStatus::OK);
                                ctx.res.setBody(std::get<std::string>(response));
                            } else if (std::holds_alternative<std::pair<HttpStatus, std::string>>(response)) {
                                auto [status, body] = std::get<std::pair<HttpStatus, std::string>>(response);
                                ctx.res.setStatus(status);
                                ctx.res.setBody(body);
                            } else if (std::holds_alternative<json>(response)) {
                                ctx.res.setJson(std::get<json>(response));
                                ctx.res.setHeader("Content-Type", "application/json");
                            } else if (std::holds_alternative<std::pair<HttpStatus, json>>(response)) {
                                auto [status, data] = std::get<std::pair<HttpStatus, json>>(response);
                                ctx.res.setStatus(status);
                                ctx.res.setJson(data);
                                ctx.res.setHeader("Content-Type", "application/json");
                            } else {
                                ctx.res.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
                                ctx.res.setBody("Internal Server Error\n");
                            }
                        } catch (const std::exception& e) {
                            ctx.res.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
                            ctx.res.setBody(std::string(e.what()) + "\n");
                        }
                    } else {
                        ctx.res.setStatus(HttpStatus::NOT_FOUND);
                        ctx.res.setBody("Not Found\n");
                    }
                } else {
                    ctx.res.setStatus(HttpStatus::METHOD_NOT_ALLOWED);
                    ctx.res.setBody("Method Not Allowed\n");
                }
            }

            sendResponse(ctx.res);
        }
    } catch (const std::exception& e) {
        try {
            Response error_response(connfd);
            error_response.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
            error_response.setBody("Server error: " + std::string(e.what()) + "\n");
            sendResponse(error_response);
        } catch (...) {
            std::cerr << "Failed to send error response: " << e.what() << std::endl;
        }
    }
}

void Server::sendResponse(const Response& response) {
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

void Server::setupServer() {
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

void Server::cleanup() {
    if (sockfd_ != -1) {
        close(sockfd_);
        sockfd_ = -1;
    }
    running_ = false;
}