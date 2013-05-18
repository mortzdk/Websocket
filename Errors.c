/******************************************************************************
  Copyright (c) 2013 Morten Houmøller Nygaard - www.mortz.dk - admin@mortz.dk
 
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

#include "Errors.h"

/**
 * This function is called when the server experience an error. This will
 * free the necessary allocations and then shutdown the server.
 */
void server_error(const char *message, int server_socket, ws_list *l) {
	shutdown(server_socket, SHUT_RD);

	printf("\nServer experienced an error: \n\t%s\nShutting down ...\n\n", 
			message);
	fflush(stdout);

	if (l != NULL) {
		list_free(l);
		l = NULL;
	}

	close(server_socket);
	exit(EXIT_FAILURE);
}

/**
 * This function is called when some error happens during the handshake. 
 * This will free all the allocations done by the specific client, send a 
 * http error status to the client, and then shut the TCP connection between 
 * client and server down.
 *
 * @param type(const char *) message [The error message for the server]
 * @param type(const char *) status [The http error status code]
 * @param type(ws_client *) n [Client]
 */
void handshake_error(const char *message, const char *status, ws_client *n) {
	printf("\nClient experienced an error: \n\t%s\nShutting him down ...\n\n", 
			message);
	fflush(stdout);	

	send(n->socket_id, status, strlen(status), 0);
	shutdown(n->socket_id, SHUT_RDWR);
	
	if (n != NULL) {
		client_free(n);
		close(n->socket_id);
		free(n);
		n = NULL;
	}
}

/**
 * This function is called when the client experience an error. This will
 * free all the allocations done by the specific client, send a closing frame
 * to the client, and then shut the TCP connection between client and server
 * down.
 *
 * @param type(const char *) message [The error message for the server]
 * @param type(ws_connection_close) c [The status of the closing]
 * @param type(ws_client *) n [Client] 
 */
void client_error(const char *message, ws_connection_close c, ws_client *n) {
	printf("\nClient experienced an error: \n\t%s\nShutting him down ...\n\n", 
			message);
	fflush(stdout);	
	
	ws_closeframe(n, c);
	shutdown(n->socket_id, SHUT_RDWR);

	if (n != NULL) {
		client_free(n);
		close(n->socket_id);
		free(n);
		n = NULL;
	}
}
