#ifndef wss_http_h
#define wss_http_h

#include "server.h"

#ifdef USE_OPENSSL
/**
 * Function initializes SSL context that can be used to serve over https.
 *
 * @param   server	[server_t *] 	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t http_ssl(server_t *server);
#endif

/**
 * Function initialized a http server instance and creating thread where the
 * instance is being run.
 *
 * @param   server	[server_t *] 	"The server instance"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t http_server(server_t *server);

/**
 * Function that free op space allocated for the http server and closes the
 * filedescriptors in use..
 *
 * @param   server	[server_t *] 	"The http server"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t http_server_free(server_t *server);

#endif
