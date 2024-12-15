#ifndef CONFIG_H
#define CONFIG_H

#include <cstddef>

namespace Config {
    // Server defaults
    constexpr const char* DEFAULT_HOST = "0.0.0.0";
    constexpr int DEFAULT_PORT = 8000;
    
    // Request/Response settings
    constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;    // 1MB
    constexpr int SOCKET_TIMEOUT = 30;                  // 30 seconds
    constexpr int BUFFER_SIZE = 8192;                   // 8KB buffer
    
    // File paths
    constexpr const char* STATIC_DIR = "static";
    constexpr const char* TEMPLATE_DIR = "templates";
}

#endif // CONFIG_H