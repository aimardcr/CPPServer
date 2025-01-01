#include <iostream>
#include <thread>
#include <cstring>
#include <regex>

#include "Defs.h"
#include "HttpServer.h"

HttpServer::HttpServer(const std::string& host, int port) 
    : host_(host), port_(port), sockfd_(INVALID_SOCK), running_(false) {
#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw ServerException("WSAStartup failed");
    }
#endif
}

HttpServer::~HttpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
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

void HttpServer::closeSocket(socket_t sock) {
    if (sock != INVALID_SOCK) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
}

std::string HttpServer::getLastError() {
#ifdef _WIN32
    wchar_t* s = NULL;
    DWORD error = WSAGetLastError();
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                  NULL, error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPWSTR)&s, 0, NULL);
    
    if (s) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
        std::string result(size_needed - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s, -1, &result[0], size_needed, NULL, NULL);
        LocalFree(s);
        return result;
    }
    return "Unknown error";
#else
    return std::string(strerror(errno));
#endif
}

void HttpServer::run() {
    if (running_) throw ServerException("Server is already running");
    
    setupServer();
    running_ = true;

    std::cout << "Listening on " << host_ << ":" << port_ << std::endl;

    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        socket_t connfd;
        do {
            connfd = accept(sockfd_, (struct sockaddr*)&client_addr, &client_addr_len);
            
            if (connfd == INVALID_SOCK) {
                #ifdef _WIN32
                    int error = WSAGetLastError();
                    if (error == WSAEWOULDBLOCK || error == WSAEINTR) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                #else
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                #endif
                
                if (running_) {
                    perror("accept");
                }
                break;
            }
        } while (connfd == INVALID_SOCK && running_);

        if (!running_) break;
        
        if (connfd != INVALID_SOCK) {
            std::thread([this, connfd]() {
                handleConnection(connfd);
                closeSocket(connfd);
            }).detach();
        }
    }

    cleanup();
}

void HttpServer::stop() {
    if (running_) {
        running_ = false;
        if (sockfd_ != INVALID_SOCK) {
            closeSocket(sockfd_);
            sockfd_ = INVALID_SOCK;
        }
    }
}

bool HttpServer::RoutePattern::match(const std::string& path, 
                                   std::map<std::string, std::string>& out_vars) const {
    std::smatch matches;
    if (!std::regex_match(path, matches, regex)) {
        return false;
    }
    
    for (size_t i = 0; i < vars.size(); i++) {
        const auto& [var_name, var_type] = vars[i];
        std::string value = matches[i + 1].str();
        
        if (var_type == "int") {
            try {
                std::stoi(value);
            } catch (const std::exception&) {
                return false;
            }
        }
        
        out_vars[var_name] = value;
    }
    
    return true;
}

HttpServer::RoutePattern::RoutePattern(const std::string& pattern) : pattern(pattern) {
    std::string regex_pattern;
    size_t pos = 0;
    
    while (pos < pattern.length()) {
        if (pattern[pos] == '{') {
            size_t end_pos = pattern.find('}', pos);
            if (end_pos == std::string::npos) {
                throw ServerException("Invalid pattern: unclosed parameter bracket");
            }
            
            std::string param = pattern.substr(pos + 1, end_pos - pos - 1);
            size_t colon_pos = param.find(':');
            
            std::string var_name;
            std::string var_type;
            
            if (colon_pos != std::string::npos) {
                var_name = param.substr(0, colon_pos);
                var_type = param.substr(colon_pos + 1);
            } else {
                var_name = param;
                var_type = "string";
            }
            
            if (!std::regex_match(var_name, std::regex("^[a-zA-Z_][a-zA-Z0-9_]*$"))) {
                throw ServerException("Invalid variable name: " + var_name);
            }
            
            vars.emplace_back(var_name, var_type);
            
            if (var_type == "int") {
                regex_pattern += "([0-9]+)";
            } else if (var_type == "string") {
                regex_pattern += "([^/]+)";
            } else {
                throw ServerException("Unsupported variable type: " + var_type);
            }
            
            pos = end_pos + 1;
        } else {
            if (std::string("{}[]()^$.|*+?\\").find(pattern[pos]) != std::string::npos) {
                regex_pattern += '\\';
            }
            regex_pattern += pattern[pos];
            pos++;
        }
    }
    
    regex = std::regex("^" + regex_pattern + "$");
}

bool HttpServer::matchRoute(const std::string& method, const std::string& path, HttpContext& ctx, AnyRouteHandler& matched_handler) {
    auto method_it = routes_.find(method);
    if (method_it != routes_.end()) {
        auto route_it = method_it->second.find(path);
        if (route_it != method_it->second.end()) {
            matched_handler = route_it->second;
            return true;
        }
    }
    
    auto pattern_method_it = pattern_routes_.find(method);
    if (pattern_method_it != pattern_routes_.end()) {
        for (const auto& [pattern, handler] : pattern_method_it->second) {
            if (pattern.match(path, ctx.path_vars.vars_)) {
                matched_handler = handler;
                return true;
            }
        }
    }
    
    return false;
}

void HttpServer::handleConnection(socket_t connfd) {
    if (Config::KEEP_ALIVE_ENABLED) {
        handleKeepAliveConnection(connfd);
        return;
    }

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
            std::string file_path = Config::STATIC_DIR + path.substr(1 + Config::STATIC_DIR.length());
            ctx.res = ctx.res.sendFile(file_path);
            sendResponse(ctx.res);
            return;
        }

        if (Config::HEALTH_CHECK_ENABLED) {
            if (method == "GET" && path == "/health") {
                ctx.res.setStatus(HttpStatus::OK);
                ctx.res.setBody("OK\n");
                sendResponse(ctx.res);
                return;
            }
        }

        AnyRouteHandler handler;
        if (!matchRoute(method, path, ctx, handler)) {
            if (routes_.find(method) == routes_.end()) {
                ctx.res.setStatus(HttpStatus::METHOD_NOT_ALLOWED);
                ctx.res.setBody("Method Not Allowed\n");
            } else {
                ctx.res.setStatus(HttpStatus::NOT_FOUND);
                ctx.res.setBody("Not Found\n");
            }
            sendResponse(ctx.res);
            return;
        }

        try {
            handler(ctx);
        } catch (const std::exception& e) {
            ctx.res.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
            ctx.res.setBody(std::string(e.what()) + "\n");
        }

        sendResponse(ctx.res);

    } catch (const std::exception& e) {
        try {
            HttpResponse error_response(connfd, ctx.req);
            error_response.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
            error_response.setBody("Server error: " + std::string(e.what()) + "\n");
            sendResponse(error_response);
        } catch (const std::exception& e2) {
            std::cerr << "Failed to send error response: " << e2.what() << std::endl;
        }
    }
}

void HttpServer::handleKeepAliveConnection(socket_t connfd) {
    int request_count = 0;
    time_t last_activity = time(nullptr);

    while (running_) {
        time_t current_time = time(nullptr);
        if (current_time - last_activity > Config::KEEP_ALIVE_TIMEOUT) {
            break;
        }

        if (request_count >= Config::MAX_KEEP_ALIVE_REQUESTS) {
            break;
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(connfd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(connfd + 1, &read_fds, NULL, NULL, &timeout);

        if (ready < 0) {
            break;
        } else if (ready == 0) {
            continue;
        }

        HttpContext ctx(connfd);

        try {
            if (!ctx.req.readRequest()) {
                break;
            }

            last_activity = time(nullptr);

            const auto& method = ctx.req.method;
            const auto& path = ctx.req.path;

            std::cout << method << " " << path << " " << ctx.req.version << std::endl;

            AnyRouteHandler handler;
            if (!matchRoute(method, path, ctx, handler)) {
                if (routes_.find(method) == routes_.end()) {
                    ctx.res.setStatus(HttpStatus::METHOD_NOT_ALLOWED);
                    ctx.res.setBody("Method Not Allowed\n");
                } else {
                    ctx.res.setStatus(HttpStatus::NOT_FOUND);
                    ctx.res.setBody("Not Found\n");
                }
                sendResponse(ctx.res);
                
                request_count++;
                
                if (ctx.req.headers.get("Connection", "") != "keep-alive") {
                    break;
                }
                continue;
            }

            try {
                handler(ctx);
            } catch (const std::exception& e) {
                ctx.res.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
                ctx.res.setBody(std::string(e.what()) + "\n");
            }

            sendResponse(ctx.res);

            request_count++;

            if (ctx.req.headers.get("Connection", "") != "keep-alive") {
                break;
            }
        } catch (const std::exception& e) {
            try {
                HttpResponse error_response(connfd, ctx.req);
                error_response.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
                error_response.setBody("Server error: " + std::string(e.what()) + "\n");
                sendResponse(error_response);
            } catch (const std::exception& e2) {
                std::cerr << "Failed to send error response: " << e2.what() << std::endl;
            }
            break;
        }
    }

    closeSocket(connfd);
}

void HttpServer::sendResponse(HttpResponse& response) {
    std::string response_str = response.toString();
    size_t total_sent = 0;
    const char* data = response_str.c_str();
    size_t remaining = response_str.length();

    while (total_sent < response_str.length()) {
        int sent = send(response.getConnfd(), 
                       data + total_sent, 
                       static_cast<int>(remaining), 
                       0);
        
        if (sent == SOCKET_ERROR) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) continue;
#else
            if (errno == EINTR) continue;
#endif
            throw ServerException("Failed to send response: " + getLastError());
        }

        total_sent += sent;
        remaining -= sent;
    }
}

void HttpServer::setupServer() {
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ == INVALID_SOCK) {
        throw ServerException("Failed to create socket: " + getLastError());
    }

#ifdef _WIN32
    char yes = 1;
#else
    int yes = 1;
#endif
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == SOCKET_ERROR) {
        closeSocket(sockfd_);
        throw ServerException("Failed to set socket options: " + getLastError());
    }

#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(sockfd_, FIONBIO, &mode) != 0) {
        closeSocket(sockfd_);
        throw ServerException("Failed to set non-blocking mode: " + getLastError());
    }
#else
    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags == -1) {
        closeSocket(sockfd_);
        throw ServerException("Failed to get socket flags: " + getLastError());
    }
    if (fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        closeSocket(sockfd_);
        throw ServerException("Failed to set non-blocking mode: " + getLastError());
    }
#endif

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port_));
    
    if (host_ == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
#ifdef _WIN32
        addr.sin_addr.s_addr = inet_addr(host_.c_str());
        if (addr.sin_addr.s_addr == INADDR_NONE) {
            closeSocket(sockfd_);
            throw ServerException("Invalid address: " + host_);
        }
#else
        if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
            closeSocket(sockfd_);
            throw ServerException("Invalid address: " + host_);
        }
#endif
    }

    if (bind(sockfd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closeSocket(sockfd_);
        throw ServerException("Failed to bind socket: " + getLastError());
    }

    if (listen(sockfd_, SOMAXCONN) == SOCKET_ERROR) {
        closeSocket(sockfd_);
        throw ServerException("Failed to listen on socket: " + getLastError());
    }
}

void HttpServer::cleanup() {
    if (sockfd_ != INVALID_SOCK) {
        closeSocket(sockfd_);
        sockfd_ = INVALID_SOCK;
    }
    running_ = false;
}