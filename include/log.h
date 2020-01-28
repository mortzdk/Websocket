#ifndef wss_log_h
#define wss_log_h

#define STRING_PORT 		"Assigning server to port "
#define STRING_BINDING 		"Binding address of server to: "
#define STRING_CONNECTED 	"User connected from ip: "
#define STRING_DISCONNECTED "User disconnected from ip: "
#define STRING_HTTP 		" using HTTP request"
#define STRING_HTTPS 		" using HTTPS request"

typedef enum {
	DISABLED     = 0,
	SERVER_ERROR = 1,
	SERVER_TRACE = 2 << 0,
	SERVER_DEBUG = 2 << 1,
	CLIENT_ERROR = 2 << 2,
	CLIENT_TRACE = 2 << 3,
	CLIENT_DEBUG = 2 << 4
} log_t;

/**
 * Function that logs whatever is given to it.
 *
 * @param   type    [log_t]     "The type of logging"
 * @param   msg     [char *]    "The error message"
 * @param   name    [char *]    "The name of the file where the log came from"
 * @param   line    [int]       "The linenumber that the log was called"
 * @return          [void]
 */
void WSS_log(log_t type, char *msg, char *name, int line);

log_t log_type;

#endif
