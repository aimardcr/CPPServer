#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <map>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <type_traits>

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

template <typename T>
Response<T> Ok(T&& data) {
    return Response<T>(HttpStatus::OK, std::forward<T>(data));
}

template <typename T>
Response<T> Created(T&& data) {
    return Response<T>(HttpStatus::CREATED, std::forward<T>(data));
}

template <typename T>
Response<T> BadRequest(T&& data) {
    return Response<T>(HttpStatus::BAD_REQUEST, std::forward<T>(data));
}

template <typename T>
Response<T> NotFound(T&& data) {
    return Response<T>(HttpStatus::NOT_FOUND, std::forward<T>(data));
}

template <typename T>
Response<T> MethodNotAllowed(T&& data) {
    return Response<T>(HttpStatus::METHOD_NOT_ALLOWED, std::forward<T>(data));
}

template <typename T>
Response<T> InternalServerError(T&& data) {
    return Response<T>(HttpStatus::INTERNAL_SERVER_ERROR, std::forward<T>(data));
}

template <typename T>
Response<T> NotImplemented(T&& data) {
    return Response<T>(HttpStatus::NOT_IMPLEMENTED, std::forward<T>(data));
}

class HttpContext {
public:
    HttpContext(socket_t connfd) : req(connfd), res(connfd, req) {}

    HttpRequest req;
    HttpResponse res;
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
    std::string host_;
    int port_;
    socket_t sockfd_;
    std::atomic<bool> running_;

#ifdef _WIN32
    WSADATA wsaData;
#endif

    using AnyRouteHandler = std::function<void(HttpContext&)>;
    std::map<std::string, std::map<std::string, AnyRouteHandler>> routes_;

    template<typename F>
    void addRouteFromHandler(const std::string& method, const std::string& path, F&& handler) {
        using ResponseType = std::invoke_result_t<F, HttpContext&>;
        using DataType = typename ResponseType::DataType;
        addRoute<DataType>(method, path, std::forward<F>(handler));
    }

    template<typename T>
    void addRoute(const std::string& method, const std::string& path, RouteHandler<T> handler) {
        routes_[method][path] = [this, handler](HttpContext& ctx) {
            Response<T> response = handler(ctx);
            handleResponse(ctx, response);
        };
    }

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
    
    void handleRequest(socket_t connfd);
    void sendResponse(HttpResponse& response);
    void setupServer();
    void cleanup();
};

#endif // HTTP_SERVER_H