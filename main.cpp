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
                if (ctx.req.params["name"].empty()) {
                    return { HttpStatus::BAD_REQUEST, "Name is required" };
                }
                
                if (ctx.req.params["name"].length() > 100) {
                    return { HttpStatus::BAD_REQUEST, "Name is too long" };
                }

                return Ok("Hello, " + ctx.req.params["name"] + "!");
            }
            return { HttpStatus::OK, "Hello, world!" };
        });

        server.get("/get-contact", [](HttpContext& ctx) -> Response<json> {
            return { json({{"name", "John Doe"}, {"email", "john.doe@mail.com"}}) };
        });        

        server.get("/set-cookie", [](HttpContext& ctx) -> Response<HttpResponse> {
            ctx.res.setCookie("name", "value");
            return ctx.res.renderTemplate("cookie.html");
        });

        server.route("/submit-data", [](HttpContext& ctx) -> Response<HttpResponse> {
            if (ctx.req.method == "GET") {
                return ctx.res.renderTemplate("submit.html");
            }

            if (ctx.req.headers.has("Content-Type")) {
                if (ctx.req.headers["Content-Type"] == "application/json") {
                    auto name = ctx.req.json.value("name", "");
                    if (name.empty()) {
                        return ctx.res.setStatus(400)
                            .setBody("Name is required");
                    }
                    auto email = ctx.req.json.value("email", "");
                    if (email.empty()) {
                        return ctx.res.setStatus(400)
                            .setBody("Email is required");
                    }
                    return ctx.res.setStatus(200)
                        .setBody("Name: " + name + ", Email: " + email);
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

        server.post("/upload-file", [](HttpContext& ctx) -> Response<std::string> {
            if (ctx.req.files.empty()) {
                return { HttpStatus::BAD_REQUEST, "No files uploaded" };
            }

            auto file = ctx.req.files;
            file["file"].save("uploads/" + file["file"].getFilename());

            return Ok("File uploaded successfully");
        });    

        server.post("/test-chunked", [](HttpContext& ctx) -> Response<std::string> {
            std::cout << "Received chunked request body:\n" << ctx.req.getBody() << std::endl;
            
            return Ok("Successfully received chunked data");
        });        

        server.run();     
    } catch (const HttpServer::ServerException& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}