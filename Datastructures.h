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

#ifndef _DATASTRUCTURES_H
#define _DATASTRUCTURES_H

#include "Includes.h"

enum state {
	WS_ERROR,
	WS_LOADING,
	WS_READING,
	WS_COMPLETE
};

struct header {
	char *host;
	char *connection;	
	char *key;
	char *key1;
	char *key2;
	char *key3;
	char *version;
	char *type;
	char *protocol;
	char *origin;
	char *upgrade;
	char *get;
	char *accept;
	char *extension;
	int host_len;
	int protocol_len;
	int origin_len;
	int upgrade_len;
	int accept_len;
	int extension_len;
	int get_len;
};

struct node {
	int socket_id;
	pthread_t thread_id;
	char *client_ip;
	char *string;
	struct header *headers;
	struct node *next;
	struct message *message;
};

struct list {
	int len;
	struct node *first;
	struct node *last;	
	pthread_mutex_t lock;
};

struct message {
	char opcode[1];
	char mask[4];
	uint64_t len;
	uint64_t enc_len; 
	uint64_t next_len;
	char *msg;
	char *next;
	char *enc;
	char *hybi00;
};
	
struct list *list_new(void);
void list_free(struct list *l);
void list_add(struct list *l, struct node *n);
void list_remove(struct list *l, struct node *r);
void list_remove_all(struct list *l);
void list_delete(struct list *l, struct node *r);
void list_print(struct list *l);
void list_multicast(struct list *l, struct node *n);
void list_multicast_one(struct list *l, struct node *n, struct message *m);
void list_multicast_all(struct list *l, struct message *m);
struct node *list_get(struct list *l, char *addr, int socket);
void list_send(struct node *n, struct message *m);

struct node *node_new(int sock, char *addr);
struct header *header_new();
struct message *message_new();

void header_free(struct header *h);
void message_free(struct message *m);
void node_free(struct node *n);
#endif
