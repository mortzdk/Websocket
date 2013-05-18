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

#ifndef _HANDSHAKE_H
#define _HANDSHAKE_H

#include "Datastructures.h"

#define ERROR_INTERNAL "HTTP/1.1 500 Internal Error\r\n\r\n"
#define ERROR_BAD "HTTP/1.1 400 Bad Request\r\n\r\n"
#define ERROR_NOT_IMPL "HTTP/1.1 501 Not Implemented\r\n\r\n"
#define ERROR_FORBIDDEN "HTTP/1.1 403 Forbidden\r\n\r\n"
#define ERROR_VERSION "HTTP/1.1 426 Upgrade Required\r\nSec-WebSocket-Version: 13, 8, 7\r\n\r\n"

#define ACCEPT_HEADER_V1 "HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
#define ACCEPT_HEADER_V1_LEN 44
#define ACCEPT_HEADER_V2 "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
#define ACCEPT_HEADER_V2_LEN 43
#define ACCEPT_HEADER_V3 "HTTP/1.1 101 Switching Protocols\r\n"
#define ACCEPT_HEADER_V3_LEN 34
#define ACCEPT_CONNECTION "Connection: Upgrade\r\n"
#define ACCEPT_CONNECTION_LEN 21
#define ACCEPT_UPGRADE "Upgrade: "
#define ACCEPT_UPGRADE_LEN 9
#define ACCEPT_KEY "Sec-WebSocket-Accept: "
#define ACCEPT_KEY_LEN 22
#define ACCEPT_PROTOCOL_V1 "WebSocket-Protocol: "
#define ACCEPT_PROTOCOL_V1_LEN 20
#define ACCEPT_PROTOCOL_V2 "Sec-WebSocket-Protocol: "
#define ACCEPT_PROTOCOL_V2_LEN 24
#define ACCEPT_ORIGIN_V1 "WebSocket-Origin: "
#define ACCEPT_ORIGIN_V1_LEN 18
#define ACCEPT_ORIGIN_V2 "Sec-WebSocket-Origin: "
#define ACCEPT_ORIGIN_V2_LEN 22
#define ACCEPT_LOCATION_V1 "WebSocket-Location: "
#define ACCEPT_LOCATION_V1_LEN 20
#define ACCEPT_LOCATION_V2 "Sec-WebSocket-Location: "
#define ACCEPT_LOCATION_V2_LEN 24

int parseHeaders(char *string, ws_client *n, int port);
int sendHandshake(ws_client *n);
#endif
