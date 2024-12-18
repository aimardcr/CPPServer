#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

#ifdef _WIN32
    using socket_t = SOCKET;
    #define INVALID_SOCK INVALID_SOCKET
#else
    using socket_t = int;
    #define INVALID_SOCK (-1)
    #define SOCKET_ERROR (-1)
#endif