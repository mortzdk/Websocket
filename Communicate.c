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

#include "Communicate.h"

/** 
 * Converts the unsigned 64 bit integer from host byte order to network byte 
 * order.
 */
uint64_t ntohl64(uint64_t value) {
	static const int num = 42;

	/**
	 * If these check is true, the system is using the little endian 
	 * convention. Else the system is using the big endian convention, which
	 * means that we do not have to represent our integers in another way.
	 */
	if (*(char *)&num == 42) {
		const uint32_t high = (uint32_t)(value >> 32);
		const uint32_t low = (uint32_t)(value & 0xFFFFFFFF);

		return (((uint64_t)(htonl(low))) << 32) | htonl(high);
	} else {
		return value;
	}	
}


/**
 * This function is suppose to get the remaining part of the message, if
 * the message from the client is too big to be contained in the buffer.
 * And we are dealing with the RFC6455 convention.
 */
uint64_t getRemainingMessage(ws_client *n, uint64_t msg_length) {
	int buffer_length = 0; 
	uint64_t remaining_length = 0, final_length = 0;
	char buffer[BUFFERSIZE];
	ws_message *m = n->message;

	do {
		memset(buffer, '\0', BUFFERSIZE);
	
		/**
		 * Receive new chunk of the message.
		 */	
		if ((buffer_length = recv(n->socket_id, buffer, BUFFERSIZE, 0)) <= 0) {
			printf("Didn't receive anything from remaining part of message. %d"
					"\n\n", buffer_length);
			fflush(stdout);
			return 0;	
		}

		/**
		 * The overall length of the message received. Because the recv call
		 * eventually will merge messages together we have to have a check
		 * whether the overall length we received is greater than the expected
		 * length of the message.
		 */ 
	 	final_length = (msg_length+remaining_length+buffer_length);	

		/**
		 * If the overall message is longer than the expected length of the
		 * message, we know that this chunk most contain the last part of the
		 * original message, and the first chunk of a new message.
		 */
		if ( final_length > m->len ) {
			uint64_t next_len = final_length-m->len;
			m->next = (char *) malloc(sizeof(char)*next_len);
			if (m->next == NULL) {
				printf("1: Couldn't allocate memory.\n\n");
				fflush(stdout);
				return 0;
			}
			memset(m->next, '\0', next_len);
		   	memcpy(m->next, buffer + (buffer_length - next_len), next_len);
			m->next_len = next_len;
			buffer_length = buffer_length - next_len;
		}

		remaining_length += buffer_length;

		memcpy(m->msg + (msg_length+(remaining_length-buffer_length)), buffer, 
				buffer_length);
	} while( (msg_length + remaining_length) < m->len );

	return remaining_length;
}

ws_connection_close parseMessage(char *buffer, uint64_t buffer_length, 
		ws_client *n) {
	ws_message *m = n->message;
	int length, has_mask, skip, j;
	uint64_t message_length = m->len, i, remaining_length = 0, buf_len;

	/**
	 * Extracting information from frame
	 */
	has_mask = buffer[1] & 0x80 ? 1 : 0;
	length = buffer[1] & 0x7f;

	if (!has_mask) {
		printf("Message didn't have masked data, received: 0x%x\n\n", 
				buffer[1]);
		fflush(stdout);
		return CLOSE_PROTOCOL;
	}

	/**
	 * We need to handle the received frame differently according to which
	 * length that the frame has set.
	 *
	 * length <= 125: We know that length is the actual length of the message,
	 * 				  and that the maskin data must be placed 2 bytes further 
	 * 				  ahead.
	 * length == 126: We know that the length is an unsigned 16 bit integer,
	 * 				  which is placed at the 2 next bytes, and that the masking
	 * 				  data must be further 2 bytes away.
	 * length == 127: We know that the length is an unsigned 64 bit integer,
	 * 				  which is placed at the 8 next bytes, and that the masking
	 * 				  data must be further 2 bytes away.
	 */
	if (length <= 125) {
		m->len += length;	
		skip = 6;
		memcpy(&m->mask, buffer + 2, sizeof(m->mask));
	} else if (length == 126) {
		uint16_t sz16;
		memcpy(&sz16, buffer + 2, sizeof(uint16_t));

		m->len += ntohs(sz16);

		skip = 8;
		memcpy(&m->mask, buffer + 4, sizeof(m->mask));
	} else if (length == 127) {
		uint64_t sz64;
		memcpy(&sz64, buffer + 2, sizeof(uint64_t));

		m->len += ntohl64(sz64);

		skip = 14;
		memcpy(&m->mask, buffer + 10, sizeof(m->mask));
	} else {
		printf("Obscure length received from client: %d\n\n", length);
		fflush(stdout);
		return CLOSE_BIG;	
	}

	/**
	 * If the message length is greater that our MAXMESSAGE constant, we
	 * skip the message and close the connection.
	 */
	if (m->len > MAXMESSAGE) {
		printf("Message received was bigger than MAXMESSAGE.");
		fflush(stdout);
		return CLOSE_BIG;
	}
	
	/**
	 * Allocating memory to hold the message sent from the client.
	 * We can do this because we now know the actual length ofr the message.
	 */ 
	m->msg = (char *) malloc(sizeof(char) * (m->len + 1));
	if (m->msg == NULL) {
		printf("2: Couldn't allocate memory.\n\n");
		fflush(stdout);
		return CLOSE_UNEXPECTED;
	}
	memset(m->msg, '\0', (m->len + 1));

	buf_len = (buffer_length-skip);

	/**
	 * The message read from recv is larger than the message we are supposed
	 * to receive. This means that we have received the first part of the next
	 * message as well.
	 */
	if (buf_len > m->len) {
		uint64_t next_len = buf_len - m->len;
		m->next = (char *) malloc(next_len);
		if (m->next == NULL) {
			printf("3: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		memset(m->next, '\0', next_len);
		memcpy(m->next, buffer + (m->len+skip), next_len);
		m->next_len = next_len;
		buf_len = m->len;	
	}

	memcpy(m->msg+message_length, buffer+skip, buf_len);

	message_length += buf_len;

	/**
	 * We have not yet received the whole message, and must continue reading
	 * new data from the client.
	 */
	if (message_length < m->len) {
		if ((remaining_length = getRemainingMessage(n, message_length)) == 0) {
			return CLOSE_POLICY;
		}
	}

	message_length += remaining_length;

	/**
	 * If this is true, our receival of the message has gone wrong, and we 
	 * have no other choice than closing the connection.
	 */
	if (message_length != m->len) {
		printf("Message does not fit. Expected: %d but got %d\n\n", 
				(int) m->len, (int) message_length);
		fflush(stdout);
		return CLOSE_POLICY;
	}

	/**
	 * If everything went well, we have to remove the masking from the data.
	 */
	for (i = 0, j = 0; i < message_length; i++, j++){
		m->msg[j] = m->msg[i] ^ m->mask[j % 4];
	}

	return CONTINUE;
}

/**
 * This function is used to get the whole message when using the Hybi-00
 * standard.
 */
ws_connection_close getWholeMessage(char *buffer, uint64_t buffer_length, 
		ws_client *n) {
	uint64_t msg_length = buffer_length, i, j;
	int buf_length;
	char buf[BUFFERSIZE];
	char *temp = NULL;

	/**
	 * Allocate what's received so far
	 */
	n->message->msg = malloc(buffer_length);
	if (n->message->msg == NULL) {
		printf("4: Couldn't allocate memory.\n\n");
		fflush(stdout);
		return CLOSE_UNEXPECTED;
	}
	memset(n->message->msg, '\0', buffer_length);

	/**
	 * If a byte is equal to zero, we know that we have reached the end of
	 * the message.
	 */
	for (i = 0; i < buffer_length; i++) {	
		if (buffer[i] != '\xFF') {
			n->message->msg[i] = buffer[i];
		} else {
			n->message->len = strlen(n->message->msg);
			return CONTINUE;
		}
	}

	/**
	 * While we still haven't seen the end of the message, continue reading
	 * data. The things done in the loop are basicly equivalent to what was
	 * done above.
	 */
	do {	
		memset(buf, '\0', BUFFERSIZE);
		if ((buf_length = recv(n->socket_id, buf, BUFFERSIZE, 0)) <= 0) {
			printf("Didn't receive any message from client.\n\n");
			fflush(stdout);
			return CLOSE_PROTOCOL;	
		}
		msg_length += buf_length;
	
		temp = realloc(n->message->msg, msg_length);
		if (temp == NULL) {
			printf("5: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		n->message->msg = temp;
		memset(n->message->msg+(msg_length-buf_length), '\0', buf_length);
		temp = NULL;

		for (j = 0, i = (msg_length-buf_length); i < msg_length; i++, j++) {	
			if (buf[j] != '\xFF') {
				n->message->msg[i] = buf[j];
			} else {
				n->message->len = strlen(n->message->msg);
				return CONTINUE;
			}
		}
	} while( msg_length < MAXMESSAGE );

	return CLOSE_UNEXPECTED;
}

/**
 * This function is split into 2. We would like to support different websocket
 * standards and therefore we encode the message as both RFC6455 and Hybi-00.
 */
ws_connection_close encodeMessage(ws_message *m) {
	uint64_t length = m->len;

	/**
	 * RFC6455 message encoding
	 */
	if (m->len <= 125) {
		length += 2;
		m->enc = (char *) malloc(sizeof(char) * length);
		if (m->enc == NULL) {
			printf("6: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		m->enc[0] = '\x81';
		m->enc[1] = m->len;
		memcpy(m->enc + 2, m->msg, m->len);
	} else if (m->len <= 65535) {
		uint16_t sz16;
		length += 4;
		m->enc = (char *) malloc(sizeof(char) * length);
		if (m->enc == NULL) {
			printf("7: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		m->enc[0] = '\x81';
		m->enc[1] = 126;
		sz16 = htons(m->len);
		memcpy(m->enc + 2, &sz16, sizeof(uint16_t));
		memcpy(m->enc + 4, m->msg, m->len);
	} else {
		uint64_t sz64;
		length += 10;
		m->enc = (char *) malloc(sizeof(char) * length);
		if (m->enc == NULL) {
			printf("8: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		m->enc[0] = '\x81';
		m->enc[1] = 127;
		sz64 = ntohl64(m->len);
		memcpy(m->enc + 2, &sz64, sizeof(uint64_t));
		memcpy(m->enc + 10, m->msg, m->len);
	}
	m->enc_len = length;

	/**
	 * Hybi-00 message encoding
	 */
	m->hybi00 = malloc(m->len+2);
	if (m->hybi00 == NULL) {
		printf("9: Couldn't allocate memory.\n\n");
		fflush(stdout);
		return CLOSE_UNEXPECTED;
	}
	memset(m->hybi00, '\0', m->len+2);
	m->hybi00[0] = 0;
	m->hybi00[m->len+1] = '\xFF';
	memcpy(m->hybi00+1, m->msg, m->len);

	return CONTINUE;
}

ws_connection_close communicate(ws_client *n, char *next, uint64_t next_len) {
	int buffer_length = 0;
	uint64_t buf_len;
	char buffer[BUFFERSIZE];
	ws_connection_close status;
	n->message = message_new();

	if (n == NULL) {
		printf("The client was not available anymore.");
		fflush(stdout);
		return CLOSE_PROTOCOL;	
	}

	if (n->headers == NULL) {
		printf("The header was not available anymore.");
		fflush(stdout);
		return CLOSE_PROTOCOL;	
	}

	/**
	 * If we are dealing with a Hypi-00 connection, we have to handle the
	 * message receiving differently than the RFC6455 standard.
	 **/	
	if ( n->headers->type == HYBI00 ) {

		memset(buffer, '\0', BUFFERSIZE);

		/**
		 * Receive new message.
		 */
		if ((buffer_length = recv(n->socket_id, buffer, BUFFERSIZE, 0)) <= 0) {
			printf("Didn't receive any message from client.\n\n");
			fflush(stdout);
			return CLOSE_PROTOCOL;
		}

		buf_len = buffer_length;

		/**
		 * If the first byte is equal to zero, the client wished to shut down.
		 * Else we keep on reading until the whole message is received.
		 */
		if (buffer[0] == '\xFF') {
			printf("Client:\n"
				  "\tSocket: %d\n"
				  "\tAddress: %s\n"
				  "reports that he is shutting down.\n\n", n->socket_id, 
				  (char *) n->client_ip);
			fflush(stdout);

			return CLOSE_NORMAL;	
		} else if (buffer[0] == '\x00') {
			/**
			 * Receive rest of the message.
			 */
			if ( (status = getWholeMessage(buffer+1, buf_len-1, n)) != 
					CONTINUE ) {
				return status; 
			}
			
			/**	
			 * Encode the message to make it ready to be send to all others.
			 */
			if ( (status = encodeMessage(n->message)) != CONTINUE) {
				return status;
			}
		}
	} else if ( n->headers->type == HYBI07 || n->headers->type == RFC6455 
			|| n->headers->type == HYBI10 ) {
		/*
		 * Receiving and decoding the message.
		 */
		do {
			memset(buffer, '\0', BUFFERSIZE);
				
			memcpy(buffer, next, next_len);

			/**
			 * If we end in this case, we have not got enough of the frame to
			 * do something useful to it. Therefore, do yet another read 
			 * operation.
			 */
			if (next_len <= 6 || ((next[1] & 0x7f) == 126 && next_len <= 8) ||
					((next[1] & 0x7f) == 127 && next_len <= 14)) {
				if ((buffer_length = recv(n->socket_id, (buffer+next_len), 
								(BUFFERSIZE-next_len), 0)) <= 0) {
					printf("Didn't receive any message from client.\n\n");
					fflush(stdout);
					return CLOSE_PROTOCOL;	
				}
			}

			buf_len = (uint64_t)(buffer_length + next_len);

			/**
			 * We need the opcode to conclude which type of message we 
			 * received.
			 */
			if (n->message->opcode[0] == '\0') {
				memcpy(n->message->opcode, buffer, sizeof(n->message->opcode));
			}

			/**
			 * Get the full message and remove the masking from it.
			 */
			if ( (status = parseMessage(buffer, buf_len, n)) != CONTINUE) {
				return status;
			}

			next_len = 0;
		} while( !(buffer[0] & 0x80) );	

		/**
		 * Checking which type of frame the client has sent.
		 */
		if (n->message->opcode[0] == '\x88' || n->message->opcode[0] == '\x08') {
			/**
			 * CLOSE: client wants to close connection, so we do.
			 **/
			printf("Client:\n"
				  "\tSocket: %d\n"
				  "\tAddress: %s\n"
				  "reports that he is shutting down.\n\n", n->socket_id, 
				  (char *) n->client_ip);
			fflush(stdout);
			
			return CLOSE_NORMAL;
		} else if (n->message->opcode[0] == '\x8A' || n->message->opcode[0] == '\x0A') {
			/**
			 * PONG: Client is still alive
			 **/
			printf("Pong arrived\n\n");
			fflush(stdout);	
			return CLOSE_TYPE;
		} else if (n->message->opcode[0] == '\x89' || n->message->opcode[0] == '\x09') {
			/** 
			 * PING: I am still alive
			 **/
			printf("Ping arrived\n\n");
			fflush(stdout);
			return CLOSE_TYPE;
		} else if (n->message->opcode[0] == '\x02' || n->message->opcode[0] == '\x82') {
			/** 
			 * BINARY: data. 
			 * TODO: find out what to do here!
			 **/
			printf("Binary data arrived\n\n");
			fflush(stdout);
			return CLOSE_TYPE;
		} else if (n->message->opcode[0] == '\x01' || n->message->opcode[0] == '\x81') {
			/** 
			 * TEXT: encode the message to make it ready to be send to all 
			 * 		 others.
			 **/
			if ( (status = encodeMessage(n->message)) != CONTINUE) {
				return status;
			}
		} else {
			printf("Something very strange happened, received opcode: 0x%x\n\n", 
					n->message->opcode[0]);
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
	}

	return 0;
}
