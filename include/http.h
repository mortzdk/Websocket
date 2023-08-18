#pragma once

#ifndef WSS_HTTP_H
#define WSS_HTTP_H

#define REQUEST_URI "^(ws%s://(%s)(:%d)?)?/(%s)?(\\?(%s)(&(%s))*)?$"

#include "server.h"

/**
 * Function that initializes a regex for the server instance that can be used 
 * to validate the connecting path of the client.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_regex_init(wss_server_t *server);

/**
 * Function that initializes a http server instance and creating thread where
 * the instance is being run.
 *
 * @param   server	[wss_server_t *] 	"The server instance"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_server(wss_server_t *server);

/**
 * Function that free op space allocated for the http server and closes the
 * filedescriptors in use..
 *
 * @param   server	[wss_server_t *] 	"The http server"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_http_server_free(wss_server_t *server);

#endif
