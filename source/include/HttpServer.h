#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <map>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <type_traits>
#include <regex>

#include "Defs.h"
#include "Config.h"

#include "HttpStatus.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

#include "json.hpp"
using json = nlohmann::json;

template <typename T>
struct Response {
    using DataType = std::conditional_t<
        std::is_array_v<std::remove_reference_t<T>> &&
        std::is_same_v<std::remove_extent_t<std::remove_reference_t<T>>, char>,
        std::string,
        std::decay_t<T>
    >;

    HttpStatus status;
    DataType data;

    template <typename U>
    Response(HttpStatus status, U&& data) 
        : status(status), data(std::forward<U>(data)) {}

    template <typename U>
    Response(int status, U&& data) 
        : status(static_cast<HttpStatus>(status)), data(std::forward<U>(data)) {}

    template <typename U>
    Response(U&& data) 
        : status(HttpStatus::OK), data(std::forward<U>(data)) {}
};

#define CreateResponseHelper(name, status) \
template <typename T> \
Response<std::string> name(T&& data) { \
    if constexpr (std::is_convertible_v<T, std::string>) { \
        return Response<std::string>(status, std::forward<T>(data)); \
    } else { \
        return Response<std::string>(status, std::to_string(std::forward<T>(data))); \
    } \
}

CreateResponseHelper(Ok, HttpStatus::OK)
CreateResponseHelper(Created, HttpStatus::CREATED)
CreateResponseHelper(BadRequest, HttpStatus::BAD_REQUEST)
CreateResponseHelper(NotFound, HttpStatus::NOT_FOUND)
CreateResponseHelper(MethodNotAllowed, HttpStatus::METHOD_NOT_ALLOWED)
CreateResponseHelper(InternalServerError, HttpStatus::INTERNAL_SERVER_ERROR)
CreateResponseHelper(NotImplemented, HttpStatus::NOT_IMPLEMENTED)

class PathVars {
public:
    const std::string& get(const std::string& name) const {
        auto it = vars_.find(name);
        if (it == vars_.end()) {
            throw std::out_of_range("Path variable not found: " + name);
        }
        return it->second;
    }
    
    int getInt(const std::string& name) const {
        return std::stoi(get(name));
    }
    
    std::string getString(const std::string& name) const {
        return get(name);
    }
    
private:
    friend class HttpServer;
    std::map<std::string, std::string> vars_;
};

class HttpContext {
public:
    HttpContext(socket_t connfd) : req(connfd), res(connfd, req) {}

    HttpRequest req;
    HttpResponse res;
    PathVars path_vars;
};

class HttpServer {
public:
    template<typename T>
    using RouteHandler = std::function<Response<T>(HttpContext&)>;

    class ServerException : public std::runtime_error {
    public:
        explicit ServerException(const std::string& msg) : std::runtime_error(msg) {}
    };

    explicit HttpServer(const std::string& host = "0.0.0.0", int port = 8000);
    ~HttpServer();

    template<typename F>
    void route(const std::string& path, F&& handler) {
        addRouteFromHandler("GET", path, std::forward<F>(handler));
        addRouteFromHandler("POST", path, std::forward<F>(handler));
        addRouteFromHandler("PUT", path, std::forward<F>(handler));
        addRouteFromHandler("PATCH", path, std::forward<F>(handler));
        addRouteFromHandler("DELETE", path, std::forward<F>(handler));
    }

    template<typename F>
    void get(const std::string& path, F&& handler) {
        addRouteFromHandler("GET", path, std::forward<F>(handler));
    }

    template<typename F>
    void post(const std::string& path, F&& handler) {
        addRouteFromHandler("POST", path, std::forward<F>(handler));
    }

    template<typename F>
    void put(const std::string& path, F&& handler) {
        addRouteFromHandler("PUT", path, std::forward<F>(handler));
    }

    template<typename F>
    void patch(const std::string& path, F&& handler) {
        addRouteFromHandler("PATCH", path, std::forward<F>(handler));
    }

    template<typename F>
    void delete_(const std::string& path, F&& handler) {
        addRouteFromHandler("DELETE", path, std::forward<F>(handler));
    }

    void setHost(const std::string& host);
    void setPort(int port);
    void run();
    void stop();

    bool isRunning() const;
    std::string getHost() const;
    int getPort() const;

private:
    struct RoutePattern {
        std::string pattern;
        std::vector<std::pair<std::string, std::string>> vars;
        std::regex regex;
        
        RoutePattern(const std::string& pattern);
        bool match(const std::string& path, std::map<std::string, std::string>& vars) const;
        
        bool operator<(const RoutePattern& other) const {
            return pattern < other.pattern;
        }
    };

    std::string host_;
    int port_;
    socket_t sockfd_;
    std::atomic<bool> running_;

#ifdef _WIN32
    WSADATA wsaData;
#endif

    using AnyRouteHandler = std::function<void(HttpContext&)>;
    std::map<std::string, std::map<std::string, AnyRouteHandler>> routes_;
    std::map<std::string, std::map<RoutePattern, AnyRouteHandler>> pattern_routes_;

    template<typename F>
    void addRouteFromHandler(const std::string& method, const std::string& path, F&& handler) {
        if (path.find('{') != std::string::npos) {
            RoutePattern pattern(path);
            using ResponseType = std::invoke_result_t<F, HttpContext&>;
            using DataType = typename ResponseType::DataType;
            
            pattern_routes_[method][pattern] = [this, handler](HttpContext& ctx) {
                Response<DataType> response = handler(ctx);
                handleResponse(ctx, response);
            };
        } else {
            using ResponseType = std::invoke_result_t<F, HttpContext&>;
            using DataType = typename ResponseType::DataType;
            addRoute<DataType>(method, path, std::forward<F>(handler));
        }
    }

    template<typename T>
    void addRoute(const std::string& method, const std::string& path, RouteHandler<T> handler) {
        routes_[method][path] = [this, handler](HttpContext& ctx) {
            Response<T> response = handler(ctx);
            handleResponse(ctx, response);
        };
    }

    bool matchRoute(const std::string& method, const std::string& path, HttpContext& ctx, AnyRouteHandler& matched_handler);

    template<typename T>
    void handleResponse(HttpContext& ctx, const Response<T>& response) {
        ctx.res.setStatus(response.status);

        if constexpr (std::is_same_v<T, json>) {
            ctx.res.setHeader("Content-Type", "application/json");
            ctx.res.setBody(response.data.dump());
        } else if constexpr (std::is_same_v<T, std::string>) {
            ctx.res.setBody(response.data);
        } else if constexpr (std::is_arithmetic_v<T>) {
            ctx.res.setBody(std::to_string(response.data));
        } else if constexpr (std::is_array_v<T> && std::is_same_v<std::remove_extent_t<T>, char>) {
            ctx.res.setBody(response.data);
        } else if constexpr (std::is_same_v<T, HttpResponse>) {
            ctx.res = response.data;
        } else {
            ctx.res.setBody(std::to_string(response.data));
        }
    }

    static void closeSocket(socket_t sock);
    static std::string getLastError();
    
    void handleConnection(socket_t connfd);
    void handleKeepAliveConnection(socket_t connfd);
    void sendResponse(HttpResponse& response);
    void setupServer();
    void cleanup();
};

#endif // HTTP_SERVER_H