#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <map>

namespace Utils {
    // String operations
    std::string trim(const std::string& str);
    std::string urlDecode(const std::string& encoded);
    std::map<std::string, std::string> parseUrlEncoded(const std::string& data);

    // File operations
    std::string readFile(const std::string& path);
}

#endif // UTILS_H