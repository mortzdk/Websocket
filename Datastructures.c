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

#include "Datastructures.h"

/**
 * Creates a new list structure.
 *
 * @return type(ws_list) [Empty list]
 */
ws_list *list_new (void) {
	ws_list *l = (ws_list *) malloc(sizeof(ws_list));
	
	if (l != NULL) {
		l->len = 0;
		l->first = l->last = NULL;

		pthread_mutex_init(&l->lock, NULL);	
	} else {
		exit(EXIT_FAILURE);
	}

	return l;
}

/**
 * Frees the list structure, including all its nodes.
 * 
 * @param type(ws_list *) l [List containing clients]
 */
void list_free (ws_list *l) {
	ws_client *n, *p;
	ws_connection_close c = CLOSE_SHUTDOWN;
	pthread_mutex_lock(&l->lock);
	n = l->first;
	
	while (n != NULL) {
		p = n->next;

		ws_closeframe(n, c);
		shutdown(n->socket_id, SHUT_RDWR);

		client_free(n);

		close(n->socket_id);
		free(n);
		n = p;
	}

	l->first = l->last = NULL;
	pthread_mutex_unlock(&l->lock);	
	pthread_mutex_destroy(&l->lock);
	free(l);
}

/**
 * Adds a node to the list l.
 *
 * @param type(ws_list *) l [List containing clients]
 * @param type(ws_client *) n [Client]
 */
void list_add (ws_list *l, ws_client *n) {
	pthread_mutex_lock(&l->lock);
	
	if (l->first != NULL) {
		l->last = l->last->next = n;	
	} else {
		l->first = l->last = n;	
	}

	l->len++;
	
	pthread_mutex_unlock(&l->lock);
}

/**
 * Removes a node from the list, and sends closing frame to the client.
 *
 * @param type(ws_list) l [List containing clients]
 * @param type(ws_client) r [Client]
 */
void list_remove (ws_list *l, ws_client *r) {
	ws_client *n, *p;
	ws_connection_close c = CLOSE_SHUTDOWN;
	pthread_mutex_lock(&l->lock);
	n = l->first;

	if (n == NULL || r == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		if (n == r) {
			if (n == l->first) {
				l->first = n->next;
			} else {
				p->next = n->next;
			}

			if (n == l->last) {
				l->last = p;
			}

			ws_closeframe(n, c);
			shutdown(n->socket_id, SHUT_RDWR);

			client_free(n);

			close(n->socket_id);
			free(n);

			l->len--;
			break;
		}
		
		p = n;
		n = n->next;
	} while (n != NULL); 

	if (l->len == 0) {
		l->first = l->last = NULL;
	} else if (l->len == 1) {
		l->last = l->first;
	}

	pthread_mutex_unlock(&l->lock);
}

/**
 * Function that will send closeframe to each client in the list.
 *
 * @param type(ws_list) l [List containing clients.]
 */
void list_remove_all (ws_list *l) {
	ws_client *n;
	ws_connection_close c = CLOSE_POLICY;

	pthread_mutex_lock(&l->lock);
	n = l->first;

	if (n == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		ws_closeframe(n, c);
		n = n->next;
	} while (n != NULL); 

	pthread_mutex_unlock(&l->lock);
}

/**
 * Prints out information about each node contained in the list.
 * 
 * @param type(ws_list *) l [List containing clients]
 */
void list_print(ws_list *l) {
	ws_client *n;
	pthread_mutex_lock(&l->lock);
	n = l->first;

	if (n == NULL) {
		printf("No clients are online.\n\n");
		fflush(stdout);
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		printf("Socket Id: \t\t%d\n"
			   "Client IP: \t\t%s\n",
			   n->socket_id, n->client_ip);
		fflush(stdout);
		n = n->next;
	} while (n != NULL);
	pthread_mutex_unlock(&l->lock);
}

/**
 * Multicasts a message to all clients, but the one given as parameter in the 
 * list
 *
 * @param type(ws_list *) l [List containing clients]
 * @param type(ws_client *) n [Client]
 */
void list_multicast(ws_list *l, ws_client *n) {
	ws_client *p;
	pthread_mutex_lock(&l->lock);
	p = l->first;
	
	if (p == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		if (p != n) {
			ws_send(p, n->message); 
		}
		p = p->next;
	} while (p != NULL);
	pthread_mutex_unlock(&l->lock);
}

/**
 * Multicasts a message to one specific client.
 *
 * @param type(ws_list *) l [List containing clients]
 * @param type(ws_client *) n [Client] 
 * @param type(ws_message *) m [Message structure, that will be sent]
 */
void list_multicast_one(ws_list *l, ws_client *n, ws_message *m) {
	ws_client *p;
	pthread_mutex_lock(&l->lock);
	p = l->first;

	if (p == NULL || n == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		if (p == n) {
			ws_send(p, m);
			break;
		}
		p = p->next;
	} while (p != NULL);
	pthread_mutex_unlock(&l->lock);
}

/**
 * Multicasts message to all client in the list.
 *
 * @param type(ws_list *) l [List containing clients]
 * @param type(ws_message *) m [Message structure, that will be sent]
 */
void list_multicast_all(ws_list *l, ws_message *m) {
	ws_client *p;
	pthread_mutex_lock(&l->lock);
	p = l->first;

	if (p == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		ws_send(p, m);
		p = p->next;
	} while (p != NULL);
	pthread_mutex_unlock(&l->lock);
}

/**
 * Returns the client that has the equivalent information as given in the 
 * parameters, if it is in the list.
 *
 * @param type(ws_list *) l [List containing clients]
 * @param type(char *) addr [The ip-address of the client]
 * @param type(int) socket [The id of the socket belonging to the client]
 * @return type(ws_client *) [Client]
 */
ws_client *list_get(ws_list *l, char *addr, int socket) {
	ws_client *p;
	pthread_mutex_lock(&l->lock);
	p = l->first;
	
	if (p == NULL) {
		pthread_mutex_unlock(&l->lock);
		return p;
	}

	do {
		if (p->socket_id == socket && strcmp(addr, p->client_ip) == 0) {
			break;
		}
		p = p->next;
	} while (p != NULL);
	pthread_mutex_unlock(&l->lock);

	return p;
}

/**
 * Functions which creates the closeframe.
 *
 * @param type(ws_client *) n [Client]
 * @param type(ws_connection_close) s [The status of the closing]
 */
void ws_closeframe(ws_client *n, ws_connection_close s) {
	char frame[2];
	(void) s;

	if (n->headers->type == RFC6455 || n->headers->type == HYBI10 || 
			n->headers->type == HYBI07) {
		frame[0] = '\x88';
		frame[1] = '\x00';
		/**
		 * TODO: 
		 * 		- Use ws_connection_close
		 */ 
		send(n->socket_id, frame, 2, 0);
	} else if (n->headers->type == HYBI00) {
		frame[0] = '\xFF';
		frame[1] = '\x00';
		send(n->socket_id, frame, 2, 0);
		pthread_cancel(n->thread_id);
	}
}

/**
 * Function which do the actual sending of messages.
 *
 * @param type(ws_client *) n [Client] 
 * @param type(ws_message *) m [Message structure, that will be sent]
 */
void ws_send(ws_client *n, ws_message *m) {
	if ( n->headers->type == HYBI00 ) {
		/**
		 * Adds 2 to the length of the message, as we have to put '\x00' and
		 * '\xFF' in the front and end of the message.
		 */
		send(n->socket_id, m->hybi00, m->len+2, 0);
	} else if ( n->headers->type == HIXIE75 ) {
		
	} else if ( n->headers->type == HYBI07 || n->headers->type == RFC6455 
			|| n->headers->type == HYBI10) {
		send(n->socket_id, m->enc, m->enc_len, 0);
	}
}

/**
 * Creates a new client.
 *
 * @param type(int) sock [The id of the clients socket]
 * @param type(char *) addr [The ip-address of the client]
 * @param type(ws_client *)
 */
ws_client *client_new (int sock, char *addr) {
	ws_client *n = (ws_client *) malloc(sizeof(ws_client));

	if (n != NULL) {
		n->socket_id = sock;		
		n->client_ip = addr;		
		n->string = NULL;
		n->thread_id = 0;
		n->headers = NULL;		
		n->message = NULL;
		n->next = NULL;
	}

	return n;
}

/**
 * Creates a new header structure.
 *
 * @return type(ws_header *) [Header structure]
 */
ws_header *header_new () {
	ws_header *h = (ws_header *) malloc(sizeof(ws_header));

	if (h != NULL) {
		h->host = NULL;
		h->connection = NULL;
		h->key = NULL;
		h->key1 = NULL;
		h->key2 = NULL;
		h->key3 = NULL;		
		h->origin = NULL;		
		h->upgrade = NULL;		
		h->get = NULL;
		h->accept = NULL;
		h->extension = NULL;
		h->resourcename = NULL;
		h->protocol_string = NULL;
		h->version = 0;
		h->host_len = 0;
		h->protocol_len = 0;
		h->origin_len = 0;
		h->upgrade_len = 0;
		h->accept_len = 0;
		h->extension_len = 0;
		h->get_len = 0;
		h->resourcename_len = 0;		
		h->type = UNKNOWN;
		h->protocol = NONE;
	}

	return h;
}

/**
 * Creates a new message structure.
 *
 * @return type(ws_message *) [Message structure]
 */
ws_message *message_new() {
	ws_message *m = (ws_message *) malloc(sizeof(ws_message));

	if (m != NULL) {
		memset(m->opcode, '\0', 1);
		memset(m->mask, '\0', 4);
		m->len = 0; 
		m->enc_len = 0;
		m->next_len = 0;
		m->msg = NULL;
		m->next = NULL;
		m->enc = NULL;
		m->hybi00 = NULL;
	}

	return m;	
}

/**
 * Frees all allocations in the header structure.
 *
 * @param type(ws_header *) h [Header structure]
 */
void header_free(ws_header *h) {
	if (h->accept != NULL) {
		free(h->accept);
		h->accept = NULL;
	}

	if (h->resourcename != NULL) {
		free(h->resourcename);
		h->resourcename = NULL;
	}

	if (h->protocol_string != NULL) {
		free(h->protocol_string);
		h->protocol_string = NULL;
	}
}

/**
 * Frees all allocations in the message structure.
 *
 * @param type(ws_message *) m [Message structure]
 */
void message_free(ws_message *m) {
	if (m->msg != NULL) {
		free(m->msg);
		m->msg = NULL;
	}

	if (m->next != NULL) {
		free(m->next);
		m->next = NULL;
	}
	
	if (m->enc != NULL) {
		free(m->enc);
		m->enc = NULL;
	}

	if (m->hybi00 != NULL) {
		free(m->hybi00);
		m->hybi00 = NULL;
	}
}

/**
 * Frees all allocations in the node, including the header and message 
 * structure.
 *
 * @param type(ws_client*) n [Client]
 */
void client_free(ws_client *n) {
	if (n->client_ip != NULL) {
		free(n->client_ip);
		n->client_ip = NULL;
	}

	if (n->string != NULL) {
		free(n->string);
		n->string = NULL;
	}

	if (n->headers != NULL) {
		header_free(n->headers);
		free(n->headers);
		n->headers = NULL;
	}

	if (n->message != NULL) {
		message_free(n->message);
		free(n->message);
		n->message = NULL;
	}
}
