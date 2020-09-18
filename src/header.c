#include <stddef.h>             /* size_t */
#include <math.h>               /* log10 */
#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen,
                                   strtok, strtok_r */

#include <sys/types.h>
#include <regex.h>              /* regex_t, regcomp, regexec */
#include <ctype.h>              /* isspace */

#include "header.h"
#include "alloc.h"
#include "b64.h"
#include "httpstatuscodes.h"
#include "session.h"
#include "log.h"
#include "str.h"
#include "subprotocols.h"
#include "extensions.h"
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
//    "HTTP/2.0",
};

/**
 * Trims a string for leading and trailing whitespace.
 *
 * @param   str     [char *]    "The string to trim"
 * @return          [char *]    "The input string without leading and trailing spaces"
 */
static inline char *trim(char* str)
{
    if ( unlikely(NULL == str) ) {
        return NULL;
    }

    if ( unlikely(str[0] == '\0') ) {
        return str;
    }
    int start, end = strlen(str);
    for (start = 0; isspace(str[start]); ++start) {}
    if (str[start]) {
        while (end > 0 && isspace(str[end-1]))
            --end;
        memmove(str, &str[start], end - start);
    }
    str[end - start] = '\0';

    return str;
}

static inline void header_set_version(wss_header_t *header, char *v) {
    int version = strtol(strtok_r(NULL, "", &v), (char **) NULL, 10);

    if ( likely(header->ws_version < version) ) {
        if ( unlikely(version == 4) ) {
            header->ws_type = HYBI04;
            header->ws_version = version;
        } else if( unlikely(version == 5) ) {
            header->ws_type = HYBI05;
            header->ws_version = version;
        } else if( unlikely(version == 6) ) {
            header->ws_type = HYBI06;
            header->ws_version = version;
        } else if( unlikely(version == 7) ) {
            header->ws_type = HYBI07;
            header->ws_version = version;
        } else if( unlikely(version == 8) ) {
            header->ws_type = HYBI10;
            header->ws_version = version;
        } else if( likely(version == 13) ) {
            header->ws_type = RFC6455;
            header->ws_version = version;
        }
    }
}


/**
 * Parses a HTTP header into a header structure and returns the status code
 * appropriate.
 *
 * @param   fd        [int]                    "The filedescriptor"
 * @param   header    [wss_header_t *]         "The header structure to fill"
 * @param   config    [wss_config_t *]         "The configuration of the server"
 * @return            [enum HttpStatus_Code]   "The status code to return to the client"
 */
enum HttpStatus_Code WSS_parse_header(int fd, wss_header_t *header, wss_config_t *config) {
    bool valid, in_use, double_clrf = false;
    size_t i, line_length;
    char *tokenptr, *lineptr, *temp, *line, *sep, *accepted;
    char *sepptr = NULL, *paramptr = NULL;
    char *token, *name; 
    wss_subprotocol_t *proto;
    wss_extension_t *ext;
    char *extensions = NULL;
    unsigned int header_size = 0;
    size_t extensions_length = 0;

    WSS_log_trace("Parsing HTTP header");

    if ( unlikely(NULL == header->content) ) {
        WSS_log_trace("No header content");
        return HttpStatus_BadRequest;
    }

    token = strtok_r(header->content, "\r", &tokenptr);

    if ( likely(tokenptr[0] == '\n') ) {
        tokenptr++;
    } else if ( unlikely(tokenptr[0] != '\0') ) {
        WSS_log_trace("Line shall always end with newline character");
        return HttpStatus_BadRequest;
    }

    if ( unlikely(NULL == token) ) {
        WSS_log_trace("No first line of header");
        return HttpStatus_BadRequest;
    }

    /**
     * Receiving and checking method of the request
     */
    if ( unlikely(NULL == (header->method = strtok_r(token, " ", &lineptr))) ) {
        WSS_log_trace("Unable to parse header method");
        return HttpStatus_BadRequest;
    }

    if ( unlikely(strinarray(header->method, methods,
                sizeof(methods)/sizeof(methods[0])) != 0) ) {
        WSS_log_trace("Method that the client is using is unknown.");
        return HttpStatus_MethodNotAllowed;
    }

    /**
     * Receiving the path of the request
     */
    if ( unlikely(NULL == (header->path = strtok_r(NULL, " ", &lineptr))) ) {
        WSS_log_trace("Unable to parse header path");
        return HttpStatus_BadRequest;
    }

    if ( unlikely(strlen(header->path) > config->size_uri) ) {
        WSS_log_trace("The size of the request URI was too large");
        return HttpStatus_URITooLong;
    } else if ( unlikely(strncmp("/", header->path, sizeof(char)*1) != 0 &&
            strncasecmp("ws://", header->path, sizeof(char)*5) != 0 &&
            strncasecmp("wss://", header->path, sizeof(char)*6) != 0 &&
            strncasecmp("http://", header->path, sizeof(char)*7) != 0 &&
            strncasecmp("https://", header->path, sizeof(char)*8) != 0) ) {
        WSS_log_trace("The request URI is not absolute URI nor relative path.");
        return HttpStatus_BadRequest;
    }

    /**
     * Receiving and checking version of the request
     */
    if ( unlikely(NULL == (header->version = strtok_r(NULL, " ", &lineptr))) ) {
        WSS_log_trace("Unable to parse header version");
        return HttpStatus_BadRequest;
    }

    if ( unlikely(strinarray(header->version, versions,
                sizeof(versions)/sizeof(versions[0])) != 0) ) {
        WSS_log_trace("HTTP version that the client is using is unknown.");
        return HttpStatus_HTTPVersionNotSupported;
    }

    // We've reached the payload
    if ( unlikely(strlen(token) >= 2 && tokenptr[0] == '\r' && tokenptr[1] == '\n') ) {
        tokenptr += 2;

        if ( unlikely(strlen(tokenptr) > config->size_payload) ) {
            WSS_log_trace("Payload size received is too large.");
            return HttpStatus_PayloadTooLarge;
        }

        double_clrf = true;
        header->payload = tokenptr;
        token = NULL;
    } else {
        token = strtok_r(NULL, "\r", &tokenptr);

        if ( likely(tokenptr[0] == '\n') ) {
            tokenptr++;
        } else if ( unlikely(tokenptr[0] != '\0') ) {
            WSS_log_trace("Line shall always end with newline character");
            return HttpStatus_BadRequest;
        }
    }

    while ( likely(NULL != token) ) {
        header_size += strlen(token);

        if ( unlikely(header_size > config->size_header) ) {
            WSS_log_trace("Header size received is too large.");
            return HttpStatus_RequestHeaderFieldsTooLarge;
        }

        line = strtok_r(token, ":", &lineptr);
        line_length = strlen(line);

        if ( likely(line != NULL) ) {
            if ( line_length == strlen(SEC_WEBSOCKET_VERSION) && 
                strncasecmp(SEC_WEBSOCKET_VERSION, line, line_length) == 0 ) {
                // The |Sec-WebSocket-Version| header field MUST NOT appear more than once in an HTTP request.
                if ( unlikely(header->ws_version != 0) ) {
                    WSS_log_trace("Sec-WebSocket-Version must only appear once in header");
                    return HttpStatus_BadRequest;
                }

                sep = trim(strtok_r(strtok_r(NULL, "", &lineptr), ",", &sepptr));
                while (NULL != sep) {
                    header_set_version(header, sep);

                    sep = trim(strtok_r(NULL, ",", &sepptr));
                }
            } else if ( line_length == strlen(UPGRADE_STRING) &&
                        strncasecmp(UPGRADE_STRING, line, line_length) == 0 ) {
                header->ws_upgrade = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( line_length == strlen(ORIGIN_STRING) &&
                        strncasecmp(ORIGIN_STRING, line, line_length) == 0 ) {
                header->ws_origin = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( line_length == strlen(CONNECTION_STRING) &&
                        strncasecmp(CONNECTION_STRING, line, line_length) == 0 ) {
                header->ws_connection = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( line_length == strlen(COOKIE_STRING) &&
                        strncasecmp(COOKIE_STRING, line, line_length) == 0 ) {
                header->cookies = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( line_length == strlen(SEC_WEBSOCKET_PROTOCOL) &&
                        strncasecmp(SEC_WEBSOCKET_PROTOCOL, line, line_length) == 0 ) {
                if (NULL == header->ws_protocol) {
                    sep = trim(strtok_r(strtok_r(NULL, "", &lineptr), ",", &sepptr));
                    if (NULL != sep && NULL != (proto = WSS_find_subprotocol(sep))) {
                        header->ws_protocol = proto;
                    } else {
                        while (NULL != (sep = trim(strtok_r(NULL, ",", &sepptr)))) {
                            if (NULL != (proto = WSS_find_subprotocol(sep))) {
                                header->ws_protocol = proto;
                                break;
                            }
                        }
                    }
                }
            } else if ( line_length == strlen(SEC_WEBSOCKET_ORIGIN) &&
                        strncasecmp(SEC_WEBSOCKET_ORIGIN, line, line_length) == 0 ) {
                header->ws_origin = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( line_length == strlen(SEC_WEBSOCKET_KEY) &&
                        strncasecmp(SEC_WEBSOCKET_KEY, line, line_length) == 0 ) {
                //The |Sec-WebSocket-Key| header field MUST NOT appear more than once in an HTTP request.
                if ( unlikely(header->ws_key != NULL) ) {
                    WSS_log_trace("Sec-WebSocket-Key must only appear once in header");
                    return HttpStatus_BadRequest;
                }
                header->ws_key = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( line_length == strlen(SEC_WEBSOCKET_EXTENSIONS) &&
                        strncasecmp(SEC_WEBSOCKET_EXTENSIONS, line, line_length) == 0 ) {
                line = trim(strtok_r(NULL, "", &lineptr));
                line_length = strlen(line);
                if ( NULL == (extensions = WSS_realloc((void **)&extensions, extensions_length, (extensions_length+line_length+1)*sizeof(char))) ) {
                    WSS_log_error("Unable to allocate space for extensions string");
                    return HttpStatus_InternalServerError;
                }

                memcpy(extensions+extensions_length, line, line_length);
                extensions_length += line_length;
            } else if ( line_length == strlen(HOST_STRING) &&
                        strncasecmp(HOST_STRING, line, line_length) == 0 ) {
                header->host = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( line_length == strlen(WEBSOCKET_PROTOCOL_STRING) &&
                        strncasecmp(WEBSOCKET_PROTOCOL_STRING, line, line_length) == 0 ) {
                header->ws_type = HIXIE75;
                if (NULL == header->ws_protocol) {
                    sep = trim(strtok_r(strtok_r(NULL, "", &lineptr), ",", &sepptr));
                    if (NULL != sep && NULL != (proto = WSS_find_subprotocol(sep))) {
                        header->ws_protocol = proto;
                    } else {
                        while (NULL != (sep = trim(strtok_r(NULL, ",", &sepptr)))) {
                            if (NULL != (proto = WSS_find_subprotocol(sep))) {
                                header->ws_protocol = proto;
                                break;
                            }
                        }
                    }
                }
            } else if ( line_length == strlen(SEC_WEBSOCKET_KEY1) &&
                        strncasecmp(SEC_WEBSOCKET_KEY1, line, line_length) == 0 ) {
                header->ws_type = HIXIE76;
                header->ws_key1 = (strtok_r(NULL, "", &lineptr)+1);
            } else if ( line_length == strlen(SEC_WEBSOCKET_KEY2) &&
                        strncasecmp(SEC_WEBSOCKET_KEY2, line, line_length) == 0 ) {
                header->ws_type = HIXIE76;
                header->ws_key2 = (strtok_r(NULL, "", &lineptr)+1);
            }
        }

        // We've reached the payload
        if ( unlikely(strlen(tokenptr) >= 2 && tokenptr[0] == '\r' && tokenptr[1] == '\n') ) {
            tokenptr += 2;

            if ( unlikely(strlen(tokenptr) > config->size_payload) ) {
                WSS_log_trace("Payload size received is too large.");
                return HttpStatus_PayloadTooLarge;
            }

            double_clrf = true;
            header->payload = tokenptr;
            temp = NULL;
        } else {
            temp = strtok_r(NULL, "\r", &tokenptr);

            if ( likely(tokenptr[0] == '\n') ) {
                tokenptr++;
            } else if ( unlikely(tokenptr[0] != '\0') ) {
                WSS_log_trace("Line shall always end with newline character");
                return HttpStatus_BadRequest;
            }
        }

        if ( unlikely(temp == NULL && header->ws_type == HIXIE76) ) {
            header->ws_type = HIXIE76;
            header->ws_key3 = header->payload;
        }

        token = temp;
    }

    if ( !double_clrf ) {
        WSS_log_trace("Double CLRF required to distinguish between header and body");
        return HttpStatus_BadRequest;
    }

    if ( NULL != extensions ) {
        sep = trim(strtok_r(extensions, ",", &sepptr));
        while(NULL != sep) {
            in_use = false;
            sep = trim(strtok_r(sep, ";", &paramptr));

            if ( NULL == (name = WSS_copy(sep, strlen(sep)+1)) ) {
                WSS_log_error("Unable to allocate space for extension structure");
                return HttpStatus_InternalServerError;
            }

            if (NULL != (ext = WSS_find_extension(name))) {
                for (i = 0; i < header->ws_extensions_count; i++) {
                    if ( unlikely(header->ws_extensions[i]->ext == ext) ) {
                        in_use = true; 
                        break;
                    }
                }

                if ( likely(! in_use) ) {
                    ext->open(fd, trim(paramptr), &accepted, &valid); 
                    if ( likely(valid) ) {
                        if ( unlikely(NULL == (header->ws_extensions = WSS_realloc((void **) &header->ws_extensions, header->ws_extensions_count*sizeof(wss_ext_t *), (header->ws_extensions_count+1)*sizeof(wss_ext_t *)))) ) {
                            WSS_log_error("Unable to allocate space for extension");
                            return HttpStatus_InternalServerError;
                        }

                        if ( unlikely(NULL == (header->ws_extensions[header->ws_extensions_count] = WSS_malloc(sizeof(wss_ext_t))))) {
                            WSS_log_error("Unable to allocate space for extension structure");
                            return HttpStatus_InternalServerError;
                        }
                        header->ws_extensions[header->ws_extensions_count]->ext = ext;
                        header->ws_extensions[header->ws_extensions_count]->name = name;
                        header->ws_extensions[header->ws_extensions_count]->accepted = accepted;
                        header->ws_extensions_count += 1;
                    }
                }
            }

            sep = trim(strtok_r(NULL, ",", &sepptr));
        }
        WSS_free((void **)&extensions);
    }

    if ( unlikely(header->ws_type == UNKNOWN
            && header->ws_version == 0
            && header->ws_key1 == NULL
            && header->ws_key2 == NULL
            && header->ws_key3 == NULL
            && header->ws_key == NULL
            && header->ws_upgrade != NULL
            && header->ws_connection != NULL
            && strlen(header->ws_upgrade) == strlen(WEBSOCKET_STRING)
            && strncasecmp(WEBSOCKET_STRING, header->ws_upgrade, strlen(WEBSOCKET_STRING)) == 0
            && strlen(header->ws_connection) == strlen(UPGRADE_STRING)
            && strncasecmp(UPGRADE_STRING, header->ws_connection, strlen(UPGRADE_STRING)) == 0
            && header->host != NULL
            && header->ws_origin != NULL) ) {
        header->ws_type = HIXIE75;
    }

    return HttpStatus_OK;
}

/**
 * Generates a regular expression pattern to match the request uri of the header.
 *
 * @param   config    [wss_config_t *]  "The configuration of the server"
 * @param   ssl       [bool]            "Whether server uses SSL"
 * @param   port      [int]             "The server port"
 * @return            [char *]          "The request uri regex pattern"
 */
static char *generate_request_uri(wss_config_t * config, bool ssl, int port) {
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

    request_uri_length += strlen(REQUEST_URI)*sizeof(char)-12*sizeof(char);
    request_uri_length += ssl*sizeof(char);
    request_uri_length += (log10(port)+1)*sizeof(char);
    request_uri_length += sum_host_length*sizeof(char);
    request_uri_length += sum_path_length*sizeof(char);
    request_uri_length += sum_query_length*sizeof(char);
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

    sprintf(request_uri, REQUEST_URI, s, host, port, path, query, query);

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
 * @param   header    [wss_header_t *]         "The header structure to fill"
 * @param   config    [wss_config_t *]         "The configuration of the server"
 * @param   ssl       [bool]                   "Whether server uses SSL"
 * @param   port      [int]                    "The server port"
 * @return            [enum HttpStatus_Code]   "The status code to return to the client"
 */
enum HttpStatus_Code WSS_upgrade_header(wss_header_t *header, wss_config_t * config, bool ssl, int port) {
    int err;
    regex_t re;
    char msg[1024];
    char *request_uri;
    char *sep, *sepptr;
    unsigned char *key = NULL;
    unsigned long key_length;

    WSS_log_trace("Upgrading HTTP header");

    if ( unlikely(strncasecmp("http://", header->path, sizeof(char)*7) == 0 ||
         strncasecmp("https://", header->path, sizeof(char)*8) == 0) ) {
        WSS_log_trace("Header path does not contain valid protocol");
        return HttpStatus_UpgradeRequired;
    }

    WSS_log_trace("Validating request_uri");
    
    // It is recommended to specify paths in the config file
    request_uri = generate_request_uri(config, ssl, port);
    if ( likely(request_uri != NULL) ) {
        if ( unlikely((err = regcomp(&re, request_uri, REG_EXTENDED|REG_NOSUB)) != 0) ) {
            regerror(err, &re, msg, 1024);
            WSS_log_error("Unable to compile regex: %s", msg);
            WSS_free((void **) &request_uri);
            return HttpStatus_InternalServerError;
        }

        err = regexec(&re, header->path, 0, NULL, 0);
        regfree(&re);
        if ( unlikely(err == REG_NOMATCH) ) {
            WSS_log_trace("Path is not allowed: %s", header->path);
            WSS_free((void **) &request_uri);
            return HttpStatus_NotFound;
        }

        if ( unlikely(err != 0) ) {
            regerror(err, &re, msg, 1024);
            WSS_log_error("Unable to exec regex: %s", msg);
            WSS_free((void **) &request_uri);
            return HttpStatus_InternalServerError;
        }

        WSS_free((void **) &request_uri);
    }

    WSS_log_trace("Validating host");

    // It is recommended to specify hosts in the config file
    if ( likely(config->hosts_length > 0) ) {
        if ( unlikely(strinarray(header->host, (const char **)config->hosts, config->hosts_length) != 0) ) {
            WSS_log_trace("Host is not allowed: %s", header->host);
            return HttpStatus_BadRequest;
        }
    }

    WSS_log_trace("Validating upgrade header");

    if ( unlikely(NULL == header->ws_upgrade || strlen(header->ws_upgrade) < strlen(WEBSOCKET_STRING) ||
            strncasecmp(WEBSOCKET_STRING, header->ws_upgrade, strlen(WEBSOCKET_STRING)) != 0) || (header->ws_type <= HIXIE76 && strncasecmp(WEBSOCKET_UPPERCASE_STRING, header->ws_upgrade, strlen(WEBSOCKET_UPPERCASE_STRING)) != 0) ) {
        WSS_log_trace("Invalid upgrade header value");
        return HttpStatus_UpgradeRequired;
    }

    WSS_log_trace("Validating connection header");

    if ( unlikely(NULL == header->ws_connection || strlen(header->ws_connection) < strlen(UPGRADE_STRING)) ) {
        WSS_log_trace("Invalid connection header value");
        return HttpStatus_UpgradeRequired;
    }

    sep = trim(strtok_r(header->ws_connection, ",", &sepptr));
    if ( unlikely(NULL == sep) ) {
        WSS_log_trace("Invalid connection header value");
        return HttpStatus_UpgradeRequired;
    }

    do {
        if ( likely(strncasecmp(UPGRADE_STRING, sep, strlen(UPGRADE_STRING)) == 0) ) {
            break;
        }

        sep = trim(strtok_r(NULL, ",", &sepptr));
    } while ( likely(NULL != sep) );

    if ( unlikely(sep == NULL) ) {
        WSS_log_trace("Invalid connection header value");
        return HttpStatus_UpgradeRequired;
    }

    WSS_log_trace("Validating origin");

    // It is recommended to specify origins in the config file
    if ( likely(config->origins_length > 0) ) {
        if ( unlikely(strinarray(header->ws_origin, (const char **)config->origins, config->origins_length) != 0) ) {
            WSS_log_trace("Origin is not allowed: %s", header->ws_origin);
            return HttpStatus_Forbidden;
        }
    }

    WSS_log_trace("Validating websocket version");
     
    switch (header->ws_type) {
        case RFC6455:
        case HYBI10:
        case HYBI07:
            break;
        default:
        WSS_log_trace("Invalid websocket version");
        return HttpStatus_NotImplemented;
    }

    WSS_log_trace("Validating websocket key");

    if ( unlikely(NULL == header->ws_key) ) {
        WSS_log_trace("Invalid websocket key");
        WSS_free((void **) &key);
        return HttpStatus_UpgradeRequired;
    }

    key = b64_decode_ex(header->ws_key, strlen(header->ws_key), &key_length);
    if ( unlikely(NULL == key || key_length != SEC_WEBSOCKET_KEY_LENGTH) ) {
        WSS_log_trace("Invalid websocket key");
        WSS_free((void **) &key);
        return HttpStatus_UpgradeRequired;
    }
    WSS_free((void **) &key);

    WSS_log_trace("Accepted handshake, switching protocol");

    return HttpStatus_SwitchingProtocols;
}

/**
 * Frees a HTTP header structure
 *
 * @param   header    [wss_header_t *]  "The HTTP header to free"
 * @return            [void]
 */
void WSS_free_header(wss_header_t *header) {
    size_t i;
    if ( likely(NULL != header) ) {
        for (i = 0; i < header->ws_extensions_count; i++) {
            if ( likely(NULL != header->ws_extensions[i]) ) {
                WSS_free((void **) &header->ws_extensions[i]->accepted);
                WSS_free((void **) &header->ws_extensions[i]->name);
                WSS_free((void **) &header->ws_extensions[i]);
            }
        }
        WSS_free((void **) &header->ws_extensions);
        WSS_free((void **) &header->content);
    }
    WSS_free((void **) &header);
}
