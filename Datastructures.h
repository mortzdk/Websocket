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

typedef enum {
	CONTINUE,
	CLOSE_NORMAL=1000, 		/* The connection */
	CLOSE_SHUTDOWN=1001, 	/* Server is shutting down */
	CLOSE_PROTOCOL=1002, 	/* Some error in the protocol has happened */
	CLOSE_TYPE=1003, 		/* The type (text, binary) was not supported */
	CLOSE_UTF8=1007, 		/* The message wasn't in UTF8 */
	CLOSE_POLICY=1008, 		/* The policy of the server has been broken */
	CLOSE_BIG=1009,			/* The messages received is too big */
	CLOSE_UNEXPECTED=1011 	/* Unexpected happened */
} ws_connection_close;

typedef enum {
	UNKNOWN,
	RFC6455,
	HYBI10,
	HYBI07,
	HYBI00,
	HIXIE75,
	HTTP
} ws_type;

typedef enum {
	NONE,
	CHAT,
	ECHO
} ws_protocol;

typedef struct {
	char *host;
	char *connection;	
	char *key;
	char *key1;
	char *key2;
	char *key3;
	char *origin;
	char *upgrade;
	char *get;
	char *accept;
	char *extension;
	char *resourcename;
	char *protocol_string;
	int version;
	int host_len;
	int protocol_len;
	int origin_len;
	int upgrade_len;
	int accept_len;
	int extension_len;
	int get_len;
	int resourcename_len;
	ws_type type;
	ws_protocol protocol;
} ws_header;

typedef struct {
	char opcode[1];
	char mask[4];
	uint64_t len;
	uint64_t enc_len; 
	uint64_t next_len;
	char *msg;
	char *next;
	char *enc;
	char *hybi00;
} ws_message;

typedef struct ws_client_n {
	int socket_id;
	char *client_ip;
	char *string;
	pthread_t thread_id;
	ws_header *headers;
	ws_message *message;
	struct ws_client_n *next;
} ws_client;

typedef struct {
	int len;
	ws_client *first;
	ws_client *last;	
	pthread_mutex_t lock;
} ws_list;

/**
 * List functions.
 */
ws_list *list_new(void);
ws_client *list_get(ws_list *l, char *addr, int socket);
void list_free(ws_list *l);
void list_add(ws_list *l, ws_client *n);
void list_remove(ws_list *l, ws_client *r);
void list_remove_all(ws_list *l);
void list_print(ws_list *l);
void list_multicast(ws_list *l, ws_client *n);
void list_multicast_one(ws_list *l, ws_client *n, ws_message *m);
void list_multicast_all(ws_list *l, ws_message *m);

/**
 * Websocket functions.
 */
void ws_closeframe(ws_client *n, ws_connection_close c);
void ws_send(ws_client *n, ws_message *m);

/**
 * New structures.
 */
ws_client *client_new(int sock, char *addr);
ws_header *header_new();
ws_message *message_new();

/**
 * Free structures
 */
void header_free(ws_header *h);
void message_free(ws_message *m);
void client_free(ws_client *n);
#endif
