#include <iostream>
#include <string>

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
            if (ctx.req.params.has("name")) {
                return Ok("Hello, " + ctx.req.params["name"] + "!");
            }
            return { HttpStatus::OK, "Hello, world!" };
        });

        server.get("/contact", [](HttpContext& ctx) -> Response<json> {
            return { json({{"name", "John Doe"}, {"email", "john.doe@mail.com"}}) };
        });

        server.get("/about", [](HttpContext& ctx) -> Response<std::string> {
            return { "This is about page." };
        });

        server.get("/cookie", [](HttpContext& ctx) -> Response<HttpResponse> {
            ctx.res.setCookie("name", "value");
            return ctx.res.renderTemplate("cookie.html");
        });

        server.get("/name", [](HttpContext& ctx) -> Response<HttpResponse> {
            return ctx.res.setStatus(200)
                .setHeader("Content-Type", "text/plain")
                .setBody("John Doe");
        });

        server.route("/submit", [](HttpContext& ctx) -> Response<HttpResponse> {
            if (ctx.req.method == "GET") {
                return ctx.res.renderTemplate("submit.html");
            }

            if (ctx.req.headers.has("Content-Type")) {
                if (ctx.req.headers["Content-Type"] == "application/json") {
                    auto name = ctx.req.json["name"];
                    if (name.empty()) {
                        return ctx.res.setStatus(400)
                            .setBody("Name is required");
                    }
                    auto email = ctx.req.json["email"];
                    if (email.empty()) {
                        return ctx.res.setStatus(400)
                            .setBody("Email is required");
                    }
                    return ctx.res.setStatus(200)
                        .setBody("Name: " + name.get<std::string>() + ", Email: " + email.get<std::string>());
                }

                if (ctx.req.headers["Content-Type"] == "application/x-www-form-urlencoded") {
                    auto name = ctx.req.forms["name"];
                    if (name.empty()) {
                        return ctx.res.setStatus(400)
                            .setBody("Name is required");
                    }
                    auto email = ctx.req.forms["email"];
                    if (email.empty()) {
                        return ctx.res.setStatus(400)
                            .setBody("Email is required");
                    }
                    return ctx.res.setStatus(200)
                        .setBody("Name: " + name + ", Email: " + email);
                }
            }

            return ctx.res.setStatus(400)
                .setBody("Bad Request");
        });

        server.get("/user/{id:int}", [](HttpContext& ctx) {
            auto userId = ctx.path_vars.getInt("id");
            return Ok("User " + std::to_string(userId));
        });        

        server.run();     
    } catch (const HttpServer::ServerException& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}