#ifndef CONFIG_H
#define CONFIG_H

#include <cstddef>
#include <string>

namespace Config {
    // Server defaults
    const std::string DEFAULT_HOST = "0.0.0.0";
    const int DEFAULT_PORT = 8000;
    
    // Request/Response settings
    const size_t MAX_REQUEST_SIZE = 1024 * 1024 * 10; // 10MB
    const int SOCKET_TIMEOUT = 30; // 30 seconds
    const int BUFFER_SIZE = 8192; // 8KB buffer
    
    // File paths
    const std::string STATIC_DIR = "static";
    const std::string TEMPLATE_DIR = "templates";
}

#endif // CONFIG_H