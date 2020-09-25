#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <stdlib.h> 			/* abs, free */
#include <string.h>             /* strerror, memset, strncpy, memcpy, strlen,
                                   strtok, strtok_r */
#include <math.h> 				/* floor, log10, abs */

#include "config.h"
#include "str.h"
#include "json.h"
#include "alloc.h"
#include "error.h"
#include "log.h"
#include "predict.h"

json_settings settings; 

static wss_error_t config_add_port_to_hosts(wss_config_t *config) {
    int n;
    unsigned int i;
    unsigned int hosts_length = config->hosts_length,
                 cur = 0,
                 digits = 0,
                 size = 0,
                 ports = 0;

    char *extra;

    if ( likely(config->port_http > 0) ) {
        digits += floor(log10(config->port_http)) + 1;
        ports++;
    }

    if ( likely(config->port_https > 0) ) {
        digits += floor(log10(config->port_https)) + 1;
        ports++;
    }

    if ( unlikely(NULL == (config->hosts = (char **)WSS_realloc(
                    (void **)&config->hosts, hosts_length*sizeof(char *), hosts_length*(ports+1)*sizeof(char *)))) ) {
        WSS_log_error("Unable to allocate new hosts");
        WSS_config_free(config);
        return WSS_MEMORY_ERROR;
    }
    config->hosts_length = hosts_length*(ports+1);

    for (i = 0; likely(i < hosts_length); i++) {
        size += strlen(config->hosts[i]);
    }

    if ( unlikely(NULL == (extra = WSS_malloc((hosts_length*digits + hosts_length*ports*2 + size*ports)*sizeof(char)))) ) {
        WSS_log_error("Unable to allocate extra");
        WSS_config_free(config);
        return WSS_MEMORY_ERROR;
    }

    if ( likely(config->port_http > 0) ) {
        for (i = 0; likely(i < hosts_length); i++) {
            if ( unlikely(0 > (n = sprintf(extra+cur, "%s:%d", config->hosts[i], config->port_http))) ) {
                WSS_log_error("Unable to perform sprintf");
                WSS_config_free(config);
                return WSS_PRINTF_ERROR;
            }
            config->hosts[hosts_length*ports+i] = extra+cur;
            cur += n+1; 
        }
        ports--;
    }

    if ( likely(config->port_https > 0) ) {
        for (i = 0; likely(i < hosts_length); i++) {
            if ( unlikely(0 > (n = sprintf(extra+cur, "%s:%d", config->hosts[i], config->port_https))) ) {
                WSS_log_error("Unable to perform sprintf");
                WSS_config_free(config);
                return WSS_PRINTF_ERROR;
            }
            config->hosts[hosts_length*ports+i] = extra+cur;
            cur += n+1; 
        }
    }

    return WSS_SUCCESS;
}

static void * WSS_config_alloc (size_t size, int zero, void * user_data)
{
   (void) user_data;
   return zero ? WSS_calloc (1, size) : WSS_malloc (size);
}

static void WSS_config_release (void * ptr, void * user_data)
{
   (void) user_data;
   WSS_free(&ptr);
}

/**
 * Loads configuration from JSON file
 *
 * @param   config  [wss_config_t *]    "The configuration structure to fill"
 * @param   path    [char *]            "The path to the JSON file"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_config_load(wss_config_t *config, char *path) {
    char error[1024];
    unsigned int i, j, length;
    char *name;
    json_value *value, *val, *temp;
    settings = (json_settings){
        .settings  = json_enable_comments,
        .mem_alloc = WSS_config_alloc,
        .mem_free  = WSS_config_release
    };

    config->length = strload(path, &config->string);
    if ( unlikely(NULL == config->string) ) {
        WSS_log_error("Unable to load JSON file: %s", error);
        WSS_config_free(config);
        return WSS_CONFIG_LOAD_ERROR;
    }

    config->data = json_parse_ex(&settings, config->string, config->length, error);

    if ( unlikely(config->data == 0) ) {
        WSS_log_error("Unable to parse JSON config: %s", error);
        WSS_config_free(config);
        return WSS_CONFIG_JSON_PARSE_ERROR;
    } else {
        if ( likely(config->data->type == json_object) ) {
            for (i = 0; likely(i < config->data->u.object.length); i++) {
                value = config->data->u.object.values[i].value;
                name = config->data->u.object.values[i].name;

                if ( strncmp(name, "hosts", 5) == 0 ) {
                    if ( likely(value->type == json_array) ) {
                        length = value->u.array.length;

                        if (length == 0) {
                            config->hosts_length = length;
                            config->hosts = NULL;
                            continue;
                        }

                        if ( unlikely(NULL == (config->hosts = WSS_calloc(length, sizeof(char *)))) ) {
                            WSS_log_error("Unable to calloc hosts");
                            WSS_config_free(config);
                            return WSS_MEMORY_ERROR;
                        }

                        for (j = 0; likely(j < length); j++) {
                            if ( likely(value->u.array.values[j]->type == json_string) ) {
                                config->hosts[j] =
                                    (char *) value->u.array.values[j]->u.string.ptr;
                            } else {
                                WSS_log_error("Invalid host");
                                WSS_config_free(config);
                                return WSS_CONFIG_INVALID_HOST;
                            }
                        }
                        config->hosts_length = length;
                    }
                } else if ( strncmp(name, "origins", 7) == 0 ) {
                    if ( likely(value->type == json_array) ) {
                        length = value->u.array.length;

                        if (length == 0) {
                            config->origins_length = length;
                            config->origins = NULL;
                            continue;
                        }

                        if ( unlikely(NULL == (config->origins = WSS_calloc(length, sizeof(char *)))) ) {
                            WSS_log_error("Unable to calloc origins");
                            WSS_config_free(config);
                            return WSS_MEMORY_ERROR;
                        }

                        for (j = 0; likely(j < length); j++) {
                            if ( likely(value->u.array.values[j]->type == json_string) ) {
                                config->origins[j] =
                                    (char *) value->u.array.values[j]->u.string.ptr;
                            } else {
                                WSS_log_error("Invalid origin");
                                WSS_config_free(config);
                                return WSS_CONFIG_INVALID_ORIGIN;
                            }
                        }
                        config->origins_length = length;
                    }
                } else if ( strncmp(name, "paths", 7) == 0 ) {
                    if ( likely(value->type == json_array) ) {
                        length = value->u.array.length;

                        if (length == 0) {
                            config->paths_length = length;
                            config->paths = NULL;
                            continue;
                        }

                        if ( unlikely(NULL == (config->paths = WSS_calloc(length, sizeof(char *)))) ) {
                            WSS_log_error("Unable to calloc paths");
                            WSS_config_free(config);
                            return WSS_MEMORY_ERROR;
                        }

                        for (j = 0; likely(j < length); j++) {
                            if ( likely(value->u.array.values[j]->type == json_string) ) {
                                config->paths[j] =
                                    (char *) value->u.array.values[j]->u.string.ptr;
                            } else {
                                WSS_log_error("Invalid path");
                                WSS_config_free(config);
                                return WSS_CONFIG_INVALID_PATH;
                            }
                        }
                        config->paths_length = length;
                    }
                } else if ( strncmp(name, "queries", 7) == 0 ) {
                    if ( likely(value->type == json_array) ) {
                        length = value->u.array.length;

                        if (length == 0) {
                            config->queries_length = length;
                            config->queries = NULL;
                            continue;
                        }

                        if ( unlikely(NULL == (config->queries = WSS_calloc(length, sizeof(char *)))) ) {
                            WSS_log_error("Unable to calloc queries");
                            WSS_config_free(config);
                            return WSS_MEMORY_ERROR;
                        }

                        for (j = 0; likely(j < length); j++) {
                            if ( likely(value->u.array.values[j]->type == json_string) ) {
                                config->queries[j] =
                                    (char *) value->u.array.values[j]->u.string.ptr;
                            } else {
                                WSS_log_error("Invalid query");
                                WSS_config_free(config);
                                return WSS_CONFIG_INVALID_QUERY;
                            }
                        }
                        config->queries_length = length;
                    }
                } else if ( strncmp(name, "setup", 5) == 0 && likely(value->type == json_object) ) {
                    if ( (val = json_value_find(value, "subprotocols")) != NULL ) {
                        if ( val->type == json_array) {
                            length = val->u.array.length;

                            if (length > 0) {
                                if ( unlikely(NULL == (config->subprotocols = WSS_calloc(length, sizeof(char *)))) ) {
                                    WSS_log_error("Unable to calloc subprotocols");
                                    WSS_config_free(config);
                                    return WSS_MEMORY_ERROR;
                                }

                                if ( unlikely(NULL == (config->subprotocols_config = WSS_calloc(length, sizeof(char *)))) ) {
                                    WSS_log_error("Unable to calloc subprotocols configuration");
                                    WSS_config_free(config);
                                    return WSS_MEMORY_ERROR;
                                }

                                for (j = 0; likely(j < length); j++) {
                                    if ( likely(val->u.array.values[j]->type == json_object) ) {
                                        if ( (temp = json_value_find(val->u.array.values[j], "file")) != NULL ) {
                                            if ( temp->type == json_string ) {
                                                config->subprotocols[j] =
                                                    (char *) temp->u.string.ptr;
                                                WSS_log_info("Found extension %s from configuration", config->subprotocols[j]);
                                            } else {
                                                WSS_log_error("Invalid subprotocol");
                                                WSS_config_free(config);
                                                return WSS_CONFIG_INVALID_SUBPROTOCOL;
                                            }
                                        }

                                        if ( (temp = json_value_find(val->u.array.values[j], "config")) != NULL ) {
                                            if ( temp->type == json_string ) {
                                                config->subprotocols_config[j] =
                                                    (char *) temp->u.string.ptr;
                                            } else {
                                                config->subprotocols_config[j] = NULL;
                                            }
                                        }
                                    } else {
                                        WSS_log_error("Invalid subprotocol");
                                        WSS_config_free(config);
                                        return WSS_CONFIG_INVALID_SUBPROTOCOL;
                                    }
                                }
                            } else {
                                config->subprotocols = NULL;
                            }

                            config->subprotocols_length = length;
                            WSS_log_info("Found %d subprotocols", length);
                        }
                    }

                    if ( (val = json_value_find(value, "extensions")) != NULL ) {
                        if ( likely(val->type == json_array) ) {
                            length = val->u.array.length;

                            if (length > 0) {
                                if ( unlikely(NULL == (config->extensions = WSS_calloc(length, sizeof(char *)))) ) {
                                    WSS_log_error("Unable to calloc extensions");
                                    WSS_config_free(config);
                                    return WSS_MEMORY_ERROR;
                                }

                                if ( unlikely(NULL == (config->extensions_config = WSS_calloc(length, sizeof(char *)))) ) {
                                    WSS_log_error("Unable to calloc extensions configuration");
                                    WSS_config_free(config);
                                    return WSS_MEMORY_ERROR;
                                }

                                for (j = 0; likely(j < length); j++) {
                                    if ( likely(val->u.array.values[j]->type == json_object) ) {
                                        if ( (temp = json_value_find(val->u.array.values[j], "file")) != NULL ) {
                                            if ( likely(temp->type == json_string) ) {
                                                config->extensions[j] =
                                                    (char *) temp->u.string.ptr;
                                                WSS_log_info("Found extension %s from configuration", config->extensions[j]);
                                            } else {
                                                WSS_log_error("Invalid extension");
                                                WSS_config_free(config);
                                                return WSS_CONFIG_INVALID_EXTENSION;
                                            }
                                        }

                                        if ( (temp = json_value_find(val->u.array.values[j], "config")) != NULL ) {
                                            if ( likely(temp->type == json_string) ) {
                                                config->extensions_config[j] =
                                                    (char *) temp->u.string.ptr;
                                            } else {
                                                config->extensions_config[j] = NULL;
                                            }
                                        }
                                    } else {
                                        WSS_log_error("Invalid extension");
                                        WSS_config_free(config);
                                        return WSS_CONFIG_INVALID_EXTENSION;
                                    }
                                }
                            } else {
                                config->extensions = NULL;
                            }

                            config->extensions_length = length;
                            WSS_log_info("Found %d extensions", length);
                        }
                    }

                    if ( (val = json_value_find(value, "favicon")) != NULL ) {
                        if ( likely(val->type == json_string) ) {
                            config->favicon = (char *)val->u.string.ptr;
                        }
                    }

                    if ( (val = json_value_find(value, "log_level")) != NULL ) {
                        if ( likely(val->type == json_integer) ) {
                            config->log = (unsigned int)val->u.integer;
                        }
                    }

                    if ( (val = json_value_find(value, "timeouts")) != NULL ) {
                        if ( likely(val->type == json_object) ) {
                            // Getting poll timeout 
                            temp = json_value_find(val, "poll");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->timeout_poll = temp->u.integer;
                            }

                            // Getting read timeout 
                            temp = json_value_find(val, "read");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->timeout_read = temp->u.integer;
                            }

                            // Getting write timeout 
                            temp = json_value_find(val, "write");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->timeout_write = temp->u.integer;
                            }

                            // Getting client timeout 
                            temp = json_value_find(val, "client");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->timeout_client = temp->u.integer;
                            }

                            // Getting amount of client pings 
                            temp = json_value_find(val, "pings");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->timeout_pings = temp->u.integer;
                            }
                        }
                    }

                    if ( (val = json_value_find(value, "ssl")) != NULL ) {
                        if ( likely(val->type == json_object) ) {
                            // Getting SSL key path
                            temp = json_value_find(val, "key");
                            if ( temp != NULL && likely(temp->type == json_string) ) {
                                config->ssl_key = (char *)temp->u.string.ptr;
                            }

                            // Getting SSL cert path
                            temp = json_value_find(val, "cert");
                            if ( temp != NULL && likely(temp->type == json_string) ) {
                                config->ssl_cert = (char *)temp->u.string.ptr;
                            }

                            // Getting SSL CA cert path
                            temp = json_value_find(val, "ca_file");
                            if ( temp != NULL && likely(temp->type == json_string) ) {
                                config->ssl_ca_file = (char *)temp->u.string.ptr;
                            }

                            // Getting SSL CA cert path
                            temp = json_value_find(val, "ca_path");
                            if ( temp != NULL && likely(temp->type == json_string) ) {
                                config->ssl_ca_path = (char *)temp->u.string.ptr;
                            }

                            // Getting whether SSL should use compression 
                            temp = json_value_find(val, "compression");
                            if ( temp != NULL && likely(temp->type == json_boolean) ) {
                                config->ssl_compression = temp->u.boolean;
                            }

                            // Getting whether peer certificate is required
                            temp = json_value_find(val, "peer_cert");
                            if ( temp != NULL && likely(temp->type == json_boolean) ) {
                                config->ssl_peer_cert = temp->u.boolean;
                            }

                            // Getting diffie helman parameters to use
                            temp = json_value_find(val, "dhparam");
                            if ( temp != NULL && likely(temp->type == json_string) ) {
                                config->ssl_dhparam = (char *)temp->u.string.ptr;
                            }

                            // Getting list of ciphers to use for SSL
                            temp = json_value_find(val, "cipher_list");
                            if ( temp != NULL && likely(temp->type == json_string) ) {
                                config->ssl_cipher_list = (char *)temp->u.string.ptr;
                            }

                            // Getting suites of ciphers to use for TLS 1.3 
                            temp = json_value_find(val, "cipher_suites");
                            if ( temp != NULL && likely(temp->type == json_string) ) {
                                config->ssl_cipher_suites = (char *)temp->u.string.ptr;
                            }
                        }
                    }

                    if ( (val = json_value_find(value, "port")) != NULL ) {
                        if ( likely(val->type == json_object) ) {
                            // Getting HTTP port
                            temp = json_value_find(val, "http");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->port_http =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting HTTPS port
                            temp = json_value_find(val, "https");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->port_https =
                                    (unsigned int)temp->u.integer;
                            }
                        }
                    }

                    if ( (val = json_value_find(value, "size")) != NULL ) {
                        if ( likely(val->type == json_object) ) {
                            // Getting buffer size
                            temp = json_value_find(val, "buffer");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->size_buffer =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting thread size
                            temp = json_value_find(val, "thread");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->size_thread =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting uri size
                            temp = json_value_find(val, "uri");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->size_uri =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting header size
                            temp = json_value_find(val, "header");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->size_header =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting ringbuffer size
                            temp = json_value_find(val, "ringbuffer");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->size_ringbuffer =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting payload size
                            temp = json_value_find(val, "payload");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->size_payload =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting frame payload size
                            temp = json_value_find(val, "frame");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->size_frame =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting max frame count for fragmented messages
                            temp = json_value_find(val, "fragmented");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->max_frames =
                                    (unsigned int)temp->u.integer;
                            }
                        }
                    }

                    if ( (val = json_value_find(value, "pool")) != NULL ) {
                        if ( likely(val->type == json_object) ) {
                            // Getting amount of workers
                            temp = json_value_find(val, "workers");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->pool_workers =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting amount of retries
                            temp = json_value_find(val, "retries");
                            if ( temp != NULL && likely(temp->type == json_integer) ) {
                                config->pool_retries =
                                    (unsigned int)temp->u.integer;
                            }
                        }
                    }
                }
            }
        } else {
            WSS_log_error("Root level of configuration is expected to be a JSON Object.");
            WSS_config_free(config);
            return WSS_CONFIG_JSON_ROOT_ERROR;
        }
    }

    if ( config->timeout_poll < 0 ) {
        config->timeout_poll = -1;
    }

    if ( config->timeout_read < 0 ) {
        config->timeout_read = -1;
    }

    if ( config->timeout_write < 0 ) {
        config->timeout_write = -1;
    }

    if ( config->timeout_client < 0 ) {
        config->timeout_client = -1;
    }

    if ( unlikely(config->port_http == 0 && config->port_https == 0) ) {
        WSS_log_error("No port chosen");
        WSS_config_free(config);
        return WSS_CONFIG_NO_PORT_ERROR;
    }

    if ( likely(config->hosts_length > 0) ) {
        return config_add_port_to_hosts(config);
    }
    
    return WSS_SUCCESS;
}

/**
 * Frees allocated memory from configuration
 *
 * @param   config  [wss_config_t *]    "The configuration structure to free"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_config_free(wss_config_t *config) {
    unsigned int amount = 1;

    if ( likely(NULL != config) ) {
        if ( likely(config->port_http > 0) ) {
            amount++;
        }

        if ( likely(config->port_https > 0) ) {
            amount++;
        }

        if ( likely(amount > 1) ) {
            if ( NULL != config->hosts ) {
                WSS_free((void **)&config->hosts[(config->hosts_length/amount)*(amount-1)]);
            }
        }

        if ( likely(NULL != config->data) ) {
            json_value_free_ex(&settings, config->data);
            config->data = NULL;
        }

        WSS_free((void **) &config->string);
        WSS_free((void **) &config->hosts);
        WSS_free((void **) &config->origins);
        WSS_free((void **) &config->queries);
        WSS_free((void **) &config->extensions);
        WSS_free((void **) &config->subprotocols);
        WSS_free((void **) &config->extensions_config);
        WSS_free((void **) &config->subprotocols_config);
    }

    return WSS_SUCCESS;
}
