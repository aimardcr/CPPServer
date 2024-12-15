#include "Utils.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <fstream>
#include <stdexcept>

namespace Utils {
    std::string trim(const std::string& str) {
        auto start = std::find_if_not(str.begin(), str.end(), ::isspace);
        auto end = std::find_if_not(str.rbegin(), str.rend(), ::isspace).base();
        return (start < end ? std::string(start, end) : std::string());
    }

    std::string urlDecode(const std::string& encoded) {
        std::string decoded;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int value;
                std::sscanf(encoded.substr(i + 1, 2).c_str(), "%x", (unsigned int *)&value);
                decoded += static_cast<char>(value);
                i += 2;
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
        std::stringstream ss(data);
        std::string pair;
        
        while (std::getline(ss, pair, '&')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = pair.substr(0, eqPos);
                std::string value = pair.substr(eqPos + 1);
                
                key = urlDecode(key);
                value = urlDecode(value);
                
                result[key] = value;
            }
        }
        return result;
    }

    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + path);
        }
        return std::string(std::istreambuf_iterator<char>(file), 
                          std::istreambuf_iterator<char>());
    }
}