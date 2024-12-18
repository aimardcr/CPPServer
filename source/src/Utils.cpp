#include <algorithm>
#include <cctype>
#include <sstream>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "Utils.h"

namespace Utils {
    std::string trim(const std::string& str) {
        auto start = std::find_if_not(str.begin(), str.end(), ::isspace);
        auto end = std::find_if_not(str.rbegin(), str.rend(), ::isspace).base();
        return (start < end ? std::string(start, end) : std::string());
    }

    std::string urlDecode(const std::string& encoded) {
        std::string decoded;
        decoded.reserve(encoded.length());

        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                unsigned int value;
                if (std::sscanf(encoded.substr(i + 1, 2).c_str(), "%x", &value) == 1) {
                    decoded += static_cast<char>(value);
                    i += 2;
                } else {
                    decoded += encoded[i];
                }
            } else if (encoded[i] == '+') {
                decoded += ' ';
            } else {
                decoded += encoded[i];
            }
        }
        return decoded;
    }

    std::map<std::string, std::string> parseUrlEncoded(const std::string& data) {
        std::map<std::string, std::string> result;
        std::istringstream ss(data);
        std::string pair;
        
        while (std::getline(ss, pair, '&')) {
            if (pair.empty()) continue;
            
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = pair.substr(0, eqPos);
                std::string value = pair.substr(eqPos + 1);
                
                key = urlDecode(trim(key));
                value = urlDecode(trim(value));
                
                result[key] = value;
            }
        }
        return result;
    }

    std::string normalizePath(const std::string& path) {
        std::string normalized = path;
    #ifdef _WIN32
        std::replace(normalized.begin(), normalized.end(), '/', '\\');
    #else
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
    #endif
        return normalized;
    }

    std::string readFile(const std::string& path) {
        std::string normalizedPath = normalizePath(path);
        
        std::ifstream file(normalizedPath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + normalizedPath);
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string content(size, '\0');
        if (!file.read(&content[0], size)) {
            throw std::runtime_error("Failed to read file: " + normalizedPath);
        }

        return content;
    }

    bool fileExists(const std::string& path) {
    #ifdef _WIN32
        DWORD attrs = GetFileAttributesA(normalizePath(path).c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    #else
        return access(normalizePath(path).c_str(), F_OK) != -1;
    #endif
    }
}