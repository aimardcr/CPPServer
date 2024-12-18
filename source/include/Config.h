#ifndef CONFIG_H
#define CONFIG_H

#include <cstddef>
#include <string>

namespace Config {
    // Server defaults
    const std::string DEFAULT_HOST = "0.0.0.0";
    const int DEFAULT_PORT = 8000;
    const bool HEALTH_CHECK_ENABLED = true;
    
    // Request/Response settings
    const size_t MAX_REQUEST_SIZE = 1024 * 1024 * 10; // 10MB
    const int SOCKET_TIMEOUT = 30; // 30 seconds
    const int BUFFER_SIZE = 8192; // 8KB buffer
    
    // File paths
    const std::string STATIC_DIR = "static";
    const std::string TEMPLATE_DIR = "templates";

    // Keep-alive settings
    constexpr bool KEEP_ALIVE_ENABLED = true;
    constexpr int KEEP_ALIVE_TIMEOUT = 5;  // 5 seconds
    constexpr int MAX_KEEP_ALIVE_REQUESTS = 100;
}

#endif // CONFIG_H