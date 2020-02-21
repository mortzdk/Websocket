#ifndef wss_error_h
#define wss_error_h

typedef enum {
    // OK
    SUCCESS                  = 0,

    // INTERRUPT
    SIGNAL                   = -1,
    
    // ERRORS
    CONFIG_ERROR             = -2,
    CONFIG_LOAD_ERROR        = -3,
    JSON_PARSE_ERROR         = -4,
    JSON_ROOT_ERROR          = -5,
    MEMORY_ERROR             = -6,
    THREAD_ERROR             = -7,
    THREADPOOL_ERROR         = -8,
    EPOLL_ERROR              = -9,
    EPOLL_CONN_ERROR         = -10,
    EPOLL_READ_ERROR         = -11,
    EPOLL_WRITE_ERROR        = -12,
    FD_ERROR                 = -13,
    LOCK_ERROR               = -14,
    SSL_ERROR                = -15,
    SOCKET_CREATE_ERROR      = -16,
    SOCKET_REUSE_ERROR       = -17,
    SOCKET_BIND_ERROR        = -18,
    SOCKET_NONBLOCKED_ERROR  = -19,
    SOCKET_LISTEN_ERROR      = -20,
    SOCKET_WAIT_ERROR        = -21,
    SIGNAL_ERROR             = -22,
} wss_error_t;

#endif
