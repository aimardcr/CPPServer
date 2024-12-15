#ifndef SERVER_H
#define SERVER_H

#include "Request.h"
#include "Response.h"
#include <string>
#include <map>
#include <functional>
#include <variant>
#include <atomic>
#include <stdexcept>

#include "json.hpp"
using json = nlohmann::json;

class Context {
public:
    Context(int connfd) : req(connfd), res(connfd) {}
    
    Request req;
    Response res;
};

class Server {
public:
    using RouteResponse = std::variant<Response, std::string, std::pair<int, std::string>, json, std::pair<int, json>>;
    using RouteHandler = std::function<RouteResponse(Context&)>;

    class ServerException : public std::runtime_error {
    public:
        explicit ServerException(const std::string& msg) : std::runtime_error(msg) {}
    };

    explicit Server(const std::string& host, int port);
    ~Server();

    void get(const std::string& path, RouteHandler handler);
    void post(const std::string& path, RouteHandler handler);
    void put(const std::string& path, RouteHandler handler);
    void patch(const std::string& path, RouteHandler handler);
    void delete_(const std::string& path, RouteHandler handler);
    void addRoute(const std::string& method, const std::string& path, RouteHandler handler);

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
    int sockfd_;
    std::atomic<bool> running_;
    std::map<std::string, std::map<std::string, RouteHandler>> routes_;

    void handleRequest(int connfd);
    void sendResponse(const Response& response);
    void setupServer();
    void cleanup();
};


#endif // SERVER_H