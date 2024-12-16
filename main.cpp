#include <iostream>
#include <string>
#include <unistd.h>

#include "HttpStatus.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpServer.h"

#include "json.hpp"
using json = nlohmann::json;

int main() {
    try {
        HttpServer server("0.0.0.0", 8000);

        server.get("/", [](HttpContext& ctx) -> Response<std::string> {
            return { HttpStatus::OK, "Hello, world!" };
        });

        server.get("/contact", [](HttpContext& ctx) -> Response<json> {
            return { json({{"name", "John Doe"}, {"email", "john.doe@mail.com"}}) };
        });

        server.get("/about", [](HttpContext& ctx) -> Response<std::string> {
            return { "This is about page." };
        });

        server.get("/cookie", [](HttpContext& ctx) -> Response<std::string> {
            ctx.res.setCookie("name", "value");
            return ctx.res.renderTemplate("cookie.html");
        });

        server.get("/name", [](HttpContext& ctx) -> Response<HttpResponse> {
            return ctx.res.setStatus(200)
                .setHeader("Content-Type", "text/plain")
                .setBody("John Doe");
        });

        server.run();     
    } catch (const HttpServer::ServerException& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}