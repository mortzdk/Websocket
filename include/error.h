#ifndef wss_error_h
#define wss_error_h

typedef enum {
    /**************************************************************************
     *                                SUCCESS                                 *
     **************************************************************************/
    WSS_SUCCESS                    = 0,
    
    /**************************************************************************
     *                                ERRORS                                  *
     **************************************************************************/

    // Unable to load configuration file
    WSS_CONFIG_LOAD_ERROR            = -1,

    // Unable to parse configurati on file as valid JSON [RFC7160]
    WSS_CONFIG_JSON_PARSE_ERROR      = -2,

    // JSON configuration must be an object at the root level
    WSS_CONFIG_JSON_ROOT_ERROR       = -3,

    // Unable to allocate memory (Out of memory)
    WSS_MEMORY_ERROR                 = -4,

    // If one of the printf functions fail
    WSS_PRINTF_ERROR                 = -5,

    // Unable to spawn a new thread
    WSS_THREAD_CREATE_ERROR          = -6,

    // Error occured in joined thread
    WSS_THREAD_JOIN_ERROR            = -7,

    // Unable to create threadpool    
    WSS_THREADPOOL_CREATE_ERROR      = -8,

    // Threadpool is full (All threads are currently working)
    WSS_THREADPOOL_FULL_ERROR        = -9,

    // Threadpool lock failed
    WSS_THREADPOOL_LOCK_ERROR        = -10,

    // Threadpool was served with invalid data
    WSS_THREADPOOL_INVALID_ERROR     = -11,

    // Threadpool is shutting down 
    WSS_THREADPOOL_SHUTDOWN_ERROR    = -12,

    // Threadpool thread returned with an error
    WSS_THREADPOOL_THREAD_ERROR      = -13,

    // Unknown threadpool error 
    WSS_THREADPOOL_ERROR             = -14,

    // Unable to initialize poll
    WSS_POLL_CREATE_ERROR            = -15,

    // Unable to add filedescriptor to poll eventslist
    WSS_POLL_SET_ERROR               = -16,

    // Unable to remove filedescriptor from poll eventslist
    WSS_POLL_REMOVE_ERROR            = -17,

    // Unknown error occured while waiting for events
    WSS_POLL_WAIT_ERROR              = -18,

    // Unknown error occured while waiting for events
    WSS_POLL_PIPE_ERROR              = -19,

    // Unable to create socket
    WSS_SOCKET_CREATE_ERROR          = -20,

    // Unable to set reuse on socket 
    WSS_SOCKET_REUSE_ERROR           = -21,

    // Unable to bind socket to address and port
    WSS_SOCKET_BIND_ERROR            = -22,

    // Unable to set socket to nonblocking
    WSS_SOCKET_NONBLOCKED_ERROR      = -23,

    // Unable to start listening on socket
    WSS_SOCKET_LISTEN_ERROR          = -24,

    // Unable to shutdown socket
    WSS_SOCKET_SHUTDOWN_ERROR        = -25,

    // Unable to shutdown socket
    WSS_SOCKET_CLOSE_ERROR           = -26,

    // Unable to set signal
    WSS_SIGNAL_ERROR                 = -27,

    // Unable to create session lock
    WSS_SESSION_LOCK_CREATE_ERROR    = -28,

    // Unable to destroy session lock
    WSS_SESSION_LOCK_DESTROY_ERROR   = -29,

    // Unable to use session lock
    WSS_SESSION_LOCK_ERROR           = -30,

    // Unable to create SSL context
    WSS_SSL_CTX_ERROR                = -31,

    // Unable to load CA certificates
    WSS_SSL_CA_ERROR                 = -32,

    // Unable to load server certificate 
    WSS_SSL_CERT_ERROR               = -33,

    // Unable to load or verify server private key 
    WSS_SSL_KEY_ERROR                = -34,

    // Unable to run ssl shutdown
    WSS_SSL_SHUTDOWN_ERROR           = -35,

    // Requires further reading to shutdown correctly
    WSS_SSL_SHUTDOWN_READ_ERROR      = -36,

    // Requires further writing to shutdown correctly
    WSS_SSL_SHUTDOWN_WRITE_ERROR     = -37,

    // Unable to read kernel filedescriptor limits
    WSS_RLIMIT_ERROR                 = -38,

    // Unable to read kernel filedescriptor limits
    WSS_RINGBUFFER_ERROR             = -39,

    // Cleanup thread failed
    WSS_CLEANUP_ERROR                = -40,
} wss_error_t;

#endif
