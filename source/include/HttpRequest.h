#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>

#include "Defs.h"
#include "SafeMap.h"
#include "Config.h"
#include "UploadedFile.h"

#include "json.hpp"

class HttpRequest {
public:
    explicit HttpRequest(socket_t fd);
    ~HttpRequest() = default;

    bool readRequest();

    std::string method;
    std::string path;
    std::string version;

    SafeMap<std::string> headers;
    SafeMap<std::string> params;
    SafeMap<std::string> forms;
    SafeMap<UploadedFile> files;
    nlohmann::json json;
    SafeMap<std::string> cookies;

private:
    std::string body;
    
    int connfd;
    bool parseHeaders(const std::string& headerData);
    void parseQueryParams();
    void parseFormData();
    void parseJsonData();
    void parseCookies();
    void parseMultipartData();

    struct MultipartPart {
        SafeMap<std::string> headers;
        std::vector<char> data;
    };

    std::string getBoundary() const;
    std::vector<MultipartPart> splitMultipartData() const;
    void processMultipartPart(const MultipartPart& part);
    SafeMap<std::string> parseContentDisposition(const std::string& header) const;

    bool readHttpRequest();
    bool setTimeout();
};

#endif // HTTP_REQUEST_H