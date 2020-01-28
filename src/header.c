#define _DEFAULT_SOURCE

#include <stddef.h>             /* size_t */
#include <math.h>               /* log10 */
#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen,
                                   strtok, strtok_r */

#include <sys/types.h>
#include <regex.h>              /* regex_t, regcomp, regexec */
#include <ctype.h>              /* isspace */

#include "header.h"
#include "base64.h"
#include "alloc.h"
#include "httpstatuscodes.h"
#include "session.h"
#include "log.h"
#include "str.h"
#include "subprotocols.h"
#include "predict.h"

/**
 * Acceptable HTTP methods
 */
const char *methods[] = {
    "GET",
//    "OPTIONS",
//    "HEAD",
//    "POST",
//    "PUT",
//    "DELETE",
//    "TRACE",
//    "CONNECT"
};

/**
 * Acceptable HTTP versions
 */
const char *versions[] = {
//    "HTTP/0.9",
//    "HTTP/1.0",
    "HTTP/1.1",
    "HTTP/2.0",
};

/**
 * Trims a string for leading and trailing whitespace.
 *
 * @param   str     [char *]    "The string to trim"
 * @return          [char *]    "The input string without leading and trailing spaces"
 */
char *trim(char *str) {
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;

    if ( unlikely(NULL == str) ) {
        return NULL;
    }

    if ( unlikely(str[0] == '\0') ) {
        return str;
    }

    len = strlen(str);
    endp = str + len;

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while (isspace((unsigned char) *frontp)) {
        ++frontp;
    }

    if (endp != frontp) {
        while (isspace((unsigned char) *(--endp)) && endp != frontp) {}
    }

    if (str + len - 1 != endp) {
        *(endp + 1) = '\0';
    } else if (frontp != str &&  endp == frontp) {
        *str = '\0';
    }

    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.  Note the reuse
     * of endp to mean the front of the string buffer now.
     */
    endp = str;
    if (frontp != str) {
        while (*frontp) {
            *endp++ = *frontp++;
        }
        *endp = '\0';
    }

    return str;
}

/**
 * Parses a HTTP header into a header structure and returns the status code
 * appropriate.
 *
 * @param   header    [header_t *]             "The header structure to fill"
 * @param   config    [config_t *]             "The configuration of the server"
 * @return            [enum HttpStatus_Code]   "The status code to return to the client"
 */
enum HttpStatus_Code WSS_parse_header(header_t *header, config_t *config) {
    char *tokenptr, *lineptr, *sepptr, *temp, *line, *sep;
    unsigned int header_size = 0;
    char *token = strtok_r(header->content, "\r\n", &tokenptr);
    wss_subprotocol_t *proto;

    if ( unlikely(NULL == token) ) {
        return HttpStatus_BadRequest;
    }

    /**
     * Receiving and checking method of the request
     */
    if ( unlikely(NULL == (header->method = strtok_r(token, " ", &lineptr))) ) {
        return HttpStatus_BadRequest;
    }

    if ( unlikely(strinarray(header->method, methods,
                sizeof(methods)/sizeof(methods[0])) != 0) ) {
        WSS_log(
                CLIENT_ERROR,
                "Method that the client is using is unknown.",
                __FILE__,
                __LINE__
               );
        return HttpStatus_MethodNotAllowed;
    }

    /**
     * Receiving the path of the request
     */
    if ( unlikely(NULL == (header->path = strtok_r(NULL, " ", &lineptr))) ) {
        return HttpStatus_BadRequest;
    }

    if ( unlikely(strlen(header->path) > config->size_uri) ) {
        WSS_log(
                CLIENT_ERROR,
                "The size of the request URI was too large",
                __FILE__,
                __LINE__
               );
        return HttpStatus_URITooLong;
    } else if ( unlikely(strncmp("/", header->path, sizeof(char)*1) != 0 &&
            strncasecmp("ws://", header->path, sizeof(char)*7) != 0 &&
            strncasecmp("wss://", header->path, sizeof(char)*7) != 0 &&
            strncasecmp("http://", header->path, sizeof(char)*7) != 0 &&
            strncasecmp("https://", header->path, sizeof(char)*8) != 0) ) {
        WSS_log(
                CLIENT_ERROR,
                "The request URI is not absolute URI nor relative path.",
                __FILE__,
                __LINE__
               );
        return HttpStatus_NotFound;
    }

    /**
     * Receiving and checking version of the request
     */
    if ( unlikely(NULL == (header->version = strtok_r(NULL, " ", &lineptr))) ) {
        return HttpStatus_BadRequest;
    }

    if ( unlikely(strinarray(header->version, versions,
                sizeof(versions)/sizeof(versions[0])) != 0) ) {
        WSS_log(
                CLIENT_ERROR,
                "HTTP version that the client is using is unknown.",
                __FILE__,
                __LINE__
               );
        return HttpStatus_HTTPVersionNotSupported;
    }

    token = strtok_r(NULL, "\r\n", &tokenptr);
    while ( likely(NULL != token) ) {
        header_size += strlen(token);

        if ( unlikely(header_size > config->size_header) ) {
            WSS_log(
                    CLIENT_ERROR,
                    "Header size received is too large.",
                    __FILE__,
                    __LINE__
                   );
            return HttpStatus_RequestHeaderFieldsTooLarge;
        }

        line = strtok_r(token, ":", &lineptr);

        if ( likely(line != NULL) ) {
            if ( strncasecmp("Sec-WebSocket-Version", line, 21) == 0 ) {
                header->ws_version = strtol(
                        (strtok_r(NULL, "", &lineptr)+1),
                        (char **) NULL,
                        10
                        );
                if ( header->ws_version == 4 ) {
                    header->ws_type = HYBI04;
                } else if( header->ws_version == 5 ) {
                    header->ws_type = HYBI05;
                } else if( header->ws_version == 6 ) {
                    header->ws_type = HYBI06;
                } else if( header->ws_version == 7 ) {
                    header->ws_type = HYBI07;
                } else if( header->ws_version == 8 ) {
                    header->ws_type = HYBI10;
                } else if( header->ws_version == 13 ) {
                    header->ws_type = RFC6455;
                }
            } else if ( strncasecmp("Upgrade", line, 7) == 0) {
                header->ws_upgrade = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( strncasecmp("Origin", line, 6) == 0 ) {
                header->ws_origin = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( strncasecmp("Connection", line, 10) == 0 ) {
                header->ws_connection = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( strncasecmp("Sec-WebSocket-Protocol", line, 22) == 0 ) {
                if (NULL == header->ws_protocol) {
                    sep = trim(strtok_r(strtok_r(NULL, "", &lineptr), ",", &sepptr));
                    if (NULL != sep && NULL != (proto = find_subprotocol(sep))) {
                        header->ws_protocol = proto;
                    } else {
                        while (NULL != (sep = trim(strtok_r(NULL, ",", &sepptr)))) {
                            if (NULL != (proto = find_subprotocol(sep))) {
                                header->ws_protocol = proto;
                                break;
                            }
                        }
                    }
                }
            } else if (strncasecmp("Sec-WebSocket-Origin", line, 20) == 0) {
                header->ws_origin = (strtok_r(NULL, "", &lineptr)+1);
            } else if (strncasecmp("Sec-WebSocket-Key", line, 17) == 0) {
                header->ws_key = (strtok_r(NULL, "", &lineptr)+1);
            } else if (strncasecmp("Sec-WebSocket-Extensions", line, 24) == 0 ) {
                // TODO: handle multiple occurences
                header->ws_extension = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( strncasecmp("Host", line, 4) == 0 ) {
                header->host = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( strncasecmp("WebSocket-Protocol", line, 18) == 0 ) {
                header->ws_type = HIXIE;
                if (NULL == header->ws_protocol) {
                    sep = trim(strtok_r(strtok_r(NULL, "", &lineptr), ",", &sepptr));
                    if (NULL != sep && NULL != (proto = find_subprotocol(sep))) {
                        header->ws_protocol = proto;
                    } else {
                        while (NULL != (sep = trim(strtok_r(NULL, ",", &sepptr)))) {
                            if (NULL != (proto = find_subprotocol(sep))) {
                                header->ws_protocol = proto;
                                break;
                            }
                        }
                    }
                }
            } else if ( strncasecmp("Sec-WebSocket-Key1", line, 18) == 0 ) {
                header->ws_type = HYBI;
                header->ws_key1 = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( strncasecmp("Sec-WebSocket-Key2", line, 18) == 0 ) {
                header->ws_type = HYBI;
                header->ws_key2 = (strtok_r(NULL, "", &lineptr)+1);
            }
        }

        tokenptr++;
        // We've reached the payload
        if ( unlikely(strlen(tokenptr) >= 2 && tokenptr[0] == '\r' && tokenptr[1] == '\n') ) {
            temp = strtok_r(NULL, "\r\n", &tokenptr);

            if ( unlikely(strlen(tokenptr) > config->size_payload) ) {
                WSS_log(
                        CLIENT_ERROR,
                        "Payload size received is too large.",
                        __FILE__,
                        __LINE__
                       );
                return HttpStatus_PayloadTooLarge;
            }

            tokenptr++;
            header->payload = tokenptr;
            temp = NULL;
        } else {
            temp = strtok_r(NULL, "\r\n", &tokenptr);
        }

        if ( unlikely(temp == NULL && header->ws_type == HYBI) ) {
            header->ws_type = HYBI;
            header->ws_key3 = token;
        }

        token = temp;
    }

    if ( unlikely(header->ws_type == UNKNOWN
            && header->ws_version == 0
            && header->ws_key1 == NULL
            && header->ws_key2 == NULL
            && header->ws_key3 == NULL
            && header->ws_key == NULL
            && header->ws_upgrade != NULL
            && header->ws_connection != NULL
            && strlen(header->ws_upgrade) == strlen(ASCII_WEBSOCKET_STRING)
            && strncasecmp(ASCII_WEBSOCKET_STRING, header->ws_upgrade, strlen(ASCII_WEBSOCKET_STRING)) == 0
            && strlen(header->ws_connection) == strlen(ASCII_CONNECTION_STRING)
            && strncasecmp(ASCII_CONNECTION_STRING, header->ws_connection, strlen(ASCII_CONNECTION_STRING)) == 0
            && header->host != NULL
            && header->ws_origin != NULL) ) {
        header->ws_type = HIXIE;
    }

    return HttpStatus_OK;
}

/**
 * Generates a regular expression pattern to match the request uri of the header.
 *
 * @param   config    [config_t *]  "The configuration of the server"
 * @param   ssl       [bool]        "Whether server uses SSL"
 * @param   port      [int]         "The server port"
 * @return            [char *]      "The request uri regex pattern"
 */
static char *generate_request_uri(config_t * config, bool ssl, int port) {
    int i, j, k;
    size_t iw = 0, jw = 0, kw = 0;
    size_t request_uri_length = 0;
    size_t sum_host_length = 0; 
    size_t sum_path_length = 0; 
    size_t sum_query_length = 0; 
    char *request_uri;
    char *host = "";
    char *path = "";
    char *query = "";
    char *s = "";

    for (i = 0; i < config->hosts_length; i++) {
        sum_host_length += strlen(config->hosts[i]); 
    }
    sum_host_length += MAX(config->hosts_length-1, 0);

    for (j = 0; j < config->paths_length; j++) {
        sum_path_length += strlen(config->paths[j]); 
    }
    sum_path_length += MAX(config->paths_length-1, 0);

    for (k = 0; k < config->queries_length; k++) {
        sum_query_length += strlen(config->queries[k]); 
    }
    sum_query_length += MAX(config->queries_length-1, 0);

    if (sum_host_length+sum_path_length+sum_query_length == 0) {
        return NULL;
    }

    request_uri_length += strlen(REQUEST_URI)*sizeof(char)-10*sizeof(char);
    request_uri_length += ssl*sizeof(char);
    request_uri_length += (log10(port)+1)*sizeof(char);
    request_uri_length += sum_host_length*sizeof(char);
    request_uri_length += sum_path_length*sizeof(char);
    request_uri_length += sum_query_length*sizeof(char);
    request_uri = (char *) WSS_malloc(request_uri_length+1*sizeof(char));

    if (ssl) {
        s = "s";
    }

    if ( unlikely(sum_host_length > 0 && NULL == (host = WSS_malloc(sum_host_length+1))) ) {
        return NULL;
    }

    if ( unlikely(sum_path_length > 0 && NULL == (path = WSS_malloc(sum_path_length+1))) ) {
        return NULL;
    }

    if ( unlikely(sum_query_length > 0 && NULL == (query = WSS_malloc(sum_query_length+1))) ) {
        return NULL;
    }

    for (i = 0; likely(i < config->hosts_length); i++) {
        if ( unlikely(i+1 == config->hosts_length) ) {
            sprintf(host+iw, "%s", config->hosts[i]);
        } else {
            sprintf(host+iw, "%s|", config->hosts[i]);
            iw++;
        }
        iw += strlen(config->hosts[i]);
    }

    for (j = 0; likely(j < config->paths_length); j++) {
        if ( unlikely(j+1 == config->paths_length) ) {
            sprintf(path+jw, "%s", config->paths[j]);
        } else {
            sprintf(path+jw, "%s|", config->paths[j]);
            jw++;
        }
        jw += strlen(config->paths[j]);
    }

    for (k = 0; likely(k < config->queries_length); k++) {
        if ( unlikely(k+1 == config->queries_length) ) {
            sprintf(query+kw, "%s", config->queries[k]);
        } else {
            sprintf(query+kw, "%s|", config->queries[k]);
            kw++;
        }
        kw += strlen(config->queries[k]);
    }

    sprintf(request_uri, REQUEST_URI, s, host, port, path, query);

    if ( likely(strlen(host) > 0) ) {
        WSS_free((void **) &host);
    }

    if ( likely(strlen(path) > 0) ) {
        WSS_free((void **) &path);
    }

    if ( likely(strlen(query) > 0) ) {
        WSS_free((void **) &query);
    }

    return request_uri;
}

/**
 * Upgrades a HTTP header, that is returns switching protocols response if
 * the header contains the required options.
 *
 * @param   header    [header_t *]             "The header structure to fill"
 * @param   config    [config_t *]             "The configuration of the server"
 * @param   ssl       [bool]                   "Whether server uses SSL"
 * @param   port      [int]                    "The server port"
 * @return            [enum HttpStatus_Code]   "The status code to return to the client"
 */
enum HttpStatus_Code WSS_upgrade_header(header_t *header, config_t * config, bool ssl, int port) {
    int err;
    regex_t re;
    char msg[1024];
    char *request_uri;
    char *key = NULL;
    char *sep, *sepptr;
    unsigned long key_length;

    if ( unlikely(strncasecmp("http://", header->path, sizeof(char)*7) == 0 ||
         strncasecmp("https://", header->path, sizeof(char)*8) == 0) ) {
        return HttpStatus_UpgradeRequired;
    }

    WSS_log(
            CLIENT_TRACE,
            "Validating request_uri",
            __FILE__,
            __LINE__
           );
    
    // It is recommended to specify paths in the config file
    request_uri = generate_request_uri(config, ssl, port);
    if ( likely(request_uri != NULL) ) {
        if ( unlikely((err = regcomp(&re, request_uri, REG_EXTENDED|REG_NOSUB)) != 0) ) {
            regerror(err, &re, msg, 1024);
            WSS_log(
                    CLIENT_ERROR,
                    msg,
                    __FILE__,
                    __LINE__
                   );

            WSS_free((void **) &request_uri);
            return HttpStatus_InternalServerError;
        }

        err = regexec(&re, header->path, 0, NULL, 0);
        regfree(&re);
        if ( unlikely(err == REG_NOMATCH) ) {
            WSS_free((void **) &request_uri);
            return HttpStatus_NotFound;
        }

        if ( unlikely(err != 0) ) {
            regerror(err, &re, msg, 1024);
            WSS_log(
                    CLIENT_ERROR,
                    msg,
                    __FILE__,
                    __LINE__
                   );

            WSS_free((void **) &request_uri);
            return HttpStatus_InternalServerError;
        }

        WSS_free((void **) &request_uri);
    }

    WSS_log(
            CLIENT_TRACE,
            "Validating host",
            __FILE__,
            __LINE__
           );

    // It is recommended to specify hosts in the config file
    if ( likely(config->hosts_length > 0) ) {
        if ( unlikely(strinarray(header->host, (const char **)config->hosts, config->hosts_length) != 0) ) {
            return HttpStatus_BadRequest;
        }
    }

    WSS_log(
            CLIENT_TRACE,
            "Validating upgrade header",
            __FILE__,
            __LINE__
           );

    if ( unlikely(NULL == header->ws_upgrade || strlen(header->ws_upgrade) < strlen(ASCII_WEBSOCKET_STRING) ||
            strncasecmp(ASCII_WEBSOCKET_STRING, header->ws_upgrade, strlen(ASCII_WEBSOCKET_STRING)) != 0) ) {
        return HttpStatus_UpgradeRequired;
    }

    WSS_log(
            CLIENT_TRACE,
            "Validating connection header",
            __FILE__,
            __LINE__
           );

    if ( unlikely(NULL == header->ws_connection || strlen(header->ws_connection) < strlen(ASCII_CONNECTION_STRING)) ) {
        return HttpStatus_UpgradeRequired;
    }

    sep = trim(strtok_r(header->ws_connection, ",", &sepptr));
    if ( unlikely(NULL == sep) ) {
        return HttpStatus_UpgradeRequired;
    }

    do {
        if ( likely(strncasecmp(ASCII_CONNECTION_STRING, sep, strlen(ASCII_CONNECTION_STRING)) == 0) ) {
            break;
        }

        sep = trim(strtok_r(NULL, ",", &sepptr));
    } while ( likely(NULL != sep) );

    if ( unlikely(sep == NULL) ) {
        return HttpStatus_UpgradeRequired;
    }

    WSS_log(
            CLIENT_TRACE,
            "Validating websocket key",
            __FILE__,
            __LINE__
           );

    if ( unlikely(NULL == header->ws_key || 
            ! base64_decode_alloc(header->ws_key, strlen(header->ws_key), &key, &key_length) ||
            key_length != SEC_WEBSOCKET_KEY_LENGTH) ) {
        WSS_free((void **) &key);
        return HttpStatus_UpgradeRequired;
    }
    WSS_free((void **) &key);

    WSS_log(
            CLIENT_TRACE,
            "Validating websocket version",
            __FILE__,
            __LINE__
           );
     
    if ( unlikely(header->ws_type != RFC6455) ) {
        return HttpStatus_UpgradeRequired;
    }

    WSS_log(
            CLIENT_TRACE,
            "Validating origin",
            __FILE__,
            __LINE__
           );

    // It is recommended to specify origins in the config file
    if ( likely(config->origins_length > 0) ) {
        if ( unlikely(strinarray(header->ws_origin, (const char **)config->origins, config->origins_length) != 0) ) {
            return HttpStatus_Forbidden;
        }
    }

    /**
     * TODO: extensions
     */
    header->ws_extension = NULL;

    WSS_log(
            CLIENT_TRACE,
            "Accepted handshake, switching protocol",
            __FILE__,
            __LINE__
           );

    return HttpStatus_SwitchingProtocols;
}

/**
 * Frees a HTTP header structure
 *
 * @param   header    [header_t *]  "The HTTP header to free"
 * @return            [void]
 */
void WSS_free_header(header_t *header) {
    WSS_free((void **) &header->content);
    WSS_free((void **) &header);
}
