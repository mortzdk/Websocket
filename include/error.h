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

    // JSON configuration must have an active port 
    WSS_CONFIG_NO_PORT_ERROR         = -4,

    // JSON configuration host must be a string
    WSS_CONFIG_INVALID_HOST          = -5,

    // JSON configuration origin must be a string
    WSS_CONFIG_INVALID_ORIGIN        = -6,

    // JSON configuration path must be a string
    WSS_CONFIG_INVALID_PATH          = -7,

    // JSON configuration query must be a string
    WSS_CONFIG_INVALID_QUERY         = -8,

    // JSON configuration subprotocol must be a string
    WSS_CONFIG_INVALID_SUBPROTOCOL   = -9,

    // JSON configuration extensions must be a string
    WSS_CONFIG_INVALID_EXTENSION     = -10,

    // Unable to allocate memory (Out of memory)
    WSS_MEMORY_ERROR                 = -11,

    // If one of the printf functions fail
    WSS_PRINTF_ERROR                 = -12,

    // Unable to spawn a new thread
    WSS_THREAD_CREATE_ERROR          = -13,

    // Error occured in joined thread
    WSS_THREAD_JOIN_ERROR            = -14,

    // Unable to create threadpool    
    WSS_THREADPOOL_CREATE_ERROR      = -15,

    // Threadpool is full (All threads are currently working)
    WSS_THREADPOOL_FULL_ERROR        = -16,

    // Threadpool lock failed
    WSS_THREADPOOL_LOCK_ERROR        = -26,

    // Threadpool was served with invalid data
    WSS_THREADPOOL_INVALID_ERROR     = -27,

    // Threadpool is shutting down 
    WSS_THREADPOOL_SHUTDOWN_ERROR    = -28,

    // Threadpool thread returned with an error
    WSS_THREADPOOL_THREAD_ERROR      = -29,

    // Unknown threadpool error 
    WSS_THREADPOOL_ERROR             = -30,

    // Unable to initialize poll
    WSS_POLL_CREATE_ERROR            = -31,

    // Unable to add filedescriptor to poll eventslist
    WSS_POLL_SET_ERROR               = -32,

    // Unable to remove filedescriptor from poll eventslist
    WSS_POLL_REMOVE_ERROR            = -33,

    // Unknown error occured while waiting for events
    WSS_POLL_WAIT_ERROR              = -34,

    // Unknown error occured while waiting for events
    WSS_POLL_PIPE_ERROR              = -35,

    // Unable to create socket
    WSS_SOCKET_CREATE_ERROR          = -36,

    // Unable to set reuse on socket 
    WSS_SOCKET_REUSE_ERROR           = -37,

    // Unable to bind socket to address and port
    WSS_SOCKET_BIND_ERROR            = -38,

    // Unable to set socket to nonblocking
    WSS_SOCKET_NONBLOCKED_ERROR      = -39,

    // Unable to start listening on socket
    WSS_SOCKET_LISTEN_ERROR          = -40,

    // Unable to shutdown socket
    WSS_SOCKET_SHUTDOWN_ERROR        = -41,

    // Unable to shutdown socket
    WSS_SOCKET_CLOSE_ERROR           = -42,

    // Unable to set signal
    WSS_SIGNAL_ERROR                 = -43,

    // Unable to create session lock
    WSS_SESSION_LOCK_CREATE_ERROR    = -44,

    // Unable to destroy session lock
    WSS_SESSION_LOCK_DESTROY_ERROR   = -45,

    // Unable to use session lock
    WSS_SESSION_LOCK_ERROR           = -46,

    // Unable to create SSL context
    WSS_SSL_CTX_ERROR                = -47,

    // Unable to load CA certificates
    WSS_SSL_CA_ERROR                 = -48,

    // Unable to load server certificate 
    WSS_SSL_CERT_ERROR               = -49,

    // Unable to load or verify server private key 
    WSS_SSL_KEY_ERROR                = -50,

    // Unable to run ssl shutdown
    WSS_SSL_SHUTDOWN_ERROR           = -51,

    // Requires further reading to shutdown correctly
    WSS_SSL_SHUTDOWN_READ_ERROR      = -52,

    // Requires further writing to shutdown correctly
    WSS_SSL_SHUTDOWN_WRITE_ERROR     = -53,

    // Unable to read kernel filedescriptor limits
    WSS_RLIMIT_ERROR                 = -54,

    // Unable to read kernel filedescriptor limits
    WSS_RINGBUFFER_ERROR             = -55,

    // Cleanup thread failed
    WSS_CLEANUP_ERROR                = -56,

    // Regex creation failed
    WSS_REGEX_ERROR                  = -57,
} wss_error_t;

#endif
