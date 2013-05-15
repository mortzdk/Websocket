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
 * Returns a new list structure.
 */
struct list *list_new (void) {
	struct list *l;

	l = (struct list *) malloc(sizeof(struct list));
	
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
 */
void list_free (struct list *l) {
	struct node *n = l->first, *p;
	pthread_mutex_lock(&l->lock);
	
	while (n != NULL) {
		p = n->next;

		node_free(n);

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
 */
void list_add (struct list *l, struct node *n) {
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
 */
void list_remove (struct list *l, struct node *r) {
	struct node *n = l->first, *p;
	pthread_mutex_lock(&l->lock);

	if (n == NULL) {
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

			node_free(n);

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
 * Removes all nodes in the list, and sends closing frame to each client.
 */
void list_remove_all (struct list *l) {
	struct node *n = l->first;
	char close[2];
	pthread_mutex_lock(&l->lock);

	if (n == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		if ( strncasecmp(n->headers->type, "hybi-07", 7) == 0 
			|| strncasecmp(n->headers->type, "RFC6455", 7) == 0 
			|| strncasecmp(n->headers->type, "hybi-10", 7) == 0 ) {
			close[0] = '\x88';
			close[1] = '\x00';
			send(n->socket_id, close, 2, 0);
		} else {
			close[0] = '\xFF';
			close[1] = '\x00';
			send(n->socket_id, close, 2, 0);
			pthread_cancel(n->thread_id);
		}
		
		n = n->next;
	} while (n != NULL); 

	pthread_mutex_unlock(&l->lock);
}

/**
 * Prints out information about each node contained in the list.
 */
void list_print(struct list *l) {
	struct node *n = l->first;
	pthread_mutex_lock(&l->lock);

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
 * Multicasts a message to all nodes but the one given as parameter in the list
 */
void list_multicast(struct list *l, struct node *n) {
	struct node *p = l->first;
	pthread_mutex_lock(&l->lock);
	
	if (p == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		if (p != n) {
			list_send(p, n->message); 
		}
		p = p->next;
	} while (p != NULL);
	pthread_mutex_unlock(&l->lock);
}

/**
 * Multicasts a message to 1 specific node.
 */
void list_multicast_one(struct list *l, struct node *n, struct message *m) {
	struct node *p = l->first;
	pthread_mutex_lock(&l->lock);
	
	if (p == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		if (p == n) {
			list_send(p, m);
			break;
		}
		p = p->next;
	} while (p != NULL);
	pthread_mutex_unlock(&l->lock);
}

/**
 * Multicasts message to alle node in the list.
 */
void list_multicast_all(struct list *l, struct message *m) {
	struct node *p = l->first;
	pthread_mutex_lock(&l->lock);
	
	if (p == NULL) {
		pthread_mutex_unlock(&l->lock);
		return;
	}

	do {
		list_send(p, m);
		p = p->next;
	} while (p != NULL);
	pthread_mutex_unlock(&l->lock);
}

/**
 * Returns the node that has the equivalent information as given in the 
 * parameters, if it is in the list.
 */
struct node *list_get(struct list *l, char *addr, int socket) {
	struct node *p = l->first;
	pthread_mutex_lock(&l->lock);
	
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
 * Function which do the actual sending of messages
 */
void list_send(struct node *n, struct message *m) {
	if (n->headers->type == NULL) {
		
	} else if ( strncasecmp(n->headers->type, "hybi-00", 7) == 0 ) {
		send(n->socket_id, m->hybi00, m->len+2, 0);
	} else if ( strncasecmp(n->headers->type, "hixie-75", 8) == 0 ) {
		
	} else if ( strncasecmp(n->headers->type, "hybi-07", 7) == 0 
			|| strncasecmp(n->headers->type, "RFC6455", 7) == 0 
			|| strncasecmp(n->headers->type, "hybi-10", 7) == 0 ) {
		send(n->socket_id, m->enc, m->enc_len, 0);
	}
}

/**
 * Creates a new node.
 */
struct node *node_new (int sock, char *addr) {
	struct node *n;

	n = (struct node *) malloc(sizeof(struct node));

	if (n != NULL) {
		n->socket_id = sock;
		n->thread_id = 0;
		n->client_ip = addr;
		n->string = NULL;
		n->headers = NULL;
		n->next = NULL;
		n->message = NULL;
	} else {
		exit(EXIT_FAILURE);
	}

	return n;
}

/**
 * Creates a new header structure.
 */
struct header *header_new () {
	struct header *h;

	h = (struct header *) malloc(sizeof(struct header));

	if (h != NULL) {
		h->host = NULL;
		h->connection = NULL;
		h->key = NULL;
		h->key1 = NULL;
		h->key2 = NULL;
		h->key3 = NULL;
		h->version = NULL;
		h->type = NULL;
		h->protocol = NULL;
		h->origin = NULL;
		h->upgrade = NULL;
		h->get = NULL;
		h->accept = NULL;
		h->extension = NULL;
		h->host_len = 0;
		h->protocol_len = 0;
		h->origin_len = 0;
		h->upgrade_len = 0;
		h->accept_len = 0;
		h->extension_len = 0;
		h->get_len = 0;
	} else {
		exit(EXIT_FAILURE);
	}

	return h;
}

/**
 * Creates a new message structure.
 */
struct message *message_new() {
	struct message *m;

	m = (struct message *) malloc(sizeof(struct message));

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
	} else {
		exit(EXIT_FAILURE);
	}

	return m;	
}

/**
 * Frees all allocations in the header structure.
 */
void header_free(struct header *h) {
	if (h->accept != NULL) {
		free(h->accept);
		h->accept = NULL;
	}
}

/**
 * Frees all allocations in the message structure.
 */
void message_free(struct message *m) {
	if (m->msg != NULL) {
		free(m->msg);
		m->msg = NULL;
	}
	
	if (m->enc != NULL) {
		free(m->enc);
		m->enc = NULL;
	}

	if (m->next != NULL) {
		free(m->next);
		m->next = NULL;
	}

	if (m->hybi00 != NULL) {
		free(m->hybi00);
		m->hybi00 = NULL;
	}
}

/**
 * Frees all allocations in the node, including the header and message 
 * structure.
 */
void node_free(struct node *n) {
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
