# C++ Server
A simple yet powerful web server made using C++

# Examples
```cpp
int main() {
    try {
        Server server("0.0.0.0", 8000);

        // Define routes easily with lambda functions
        server.get("/", [&](Context& ctx) -> Server::RouteResponse {
            return ctx.res.renderTemplate("index.html");
        });

        // Prefer this style for returning responses?
        server.get("/api/users", [&](Context& ctx) -> Server::RouteResponse {
            return ctx.res.setStatus(200)
                          .setJson(json::array({
                                {{"id", 1}, {"name", "Alice"}},
                                {{"id", 2}, {"name", "Bob"}}
                          }));
        });

        // Or good ol' status code and body.
        server.get("/api/users/1", [&](Context& ctx) -> Server::RouteResponse {
            return std::make_pair(200, json({{"id", 1}, {"name", "Alice"}}));
        });

        // Or even better, without status codes!
        server.get("/api/users/2", [&](Context& ctx) -> Server::RouteResponse {;
            return json({{"id", 2}, {"name", "Bob"}});
        });

        server.run();     
    } catch (const Server::ServerException& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

# Build
Run `make` to build the project, the binary will be in the `server` directory or run `make run` to start the server immediately.

# TODO
So much... please kindly check the source and any contributors are welcome!
