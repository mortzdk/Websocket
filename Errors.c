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

void server_error(const char *message, int server_socket, struct list *l, 
		struct list *j) {
	printf("\nServer experienced an error: %s\n"
   		   "Shutting down ...\n\n", message);
	fflush(stdout);
	
	if (j != NULL) {
		list_free(j);
		j = NULL;
	}

	if (l != NULL) {
		list_free(l);
		l = NULL;
	}

	close(server_socket);
	exit(EXIT_FAILURE);
}

void client_error(const char *errormessage, const char *status, 
		struct node *n) {
	
	printf("\nClient experienced an error: %s\n"
		   "Shutting him down ...\n\n", errormessage);
	fflush(stdout);	

	send(n->socket_id, status, strlen(status), 0);
	
	if (n != NULL) {
		node_free(n);

		close(n->socket_id);
		free(n);
		n = NULL;
	}
}
