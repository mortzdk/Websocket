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

static void config_add_port_to_hosts(config_t *config) {
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
        return;
    }
    config->hosts_length = hosts_length*(ports+1);

    for (i = 0; likely(i < hosts_length); i++) {
        size += strlen(config->hosts[i]);
    }

    if ( unlikely(NULL == (extra = WSS_malloc((hosts_length*digits + hosts_length*ports*2 + size*ports)*sizeof(char)))) ) {
        return;
    }

    if ( likely(config->port_http > 0) ) {
        for (i = 0; likely(i < hosts_length); i++) {
            if ( unlikely(0 > (n = sprintf(extra+cur, "%s:%d", config->hosts[i], config->port_http))) ) {
                return;
            }
            config->hosts[hosts_length*ports+i] = extra+cur;
            cur += n+1; 
        }
        ports--;
    }

    if ( likely(config->port_https > 0) ) {
        for (i = 0; likely(i < hosts_length); i++) {
            if ( unlikely(0 > (n = sprintf(extra+cur, "%s:%d", config->hosts[i], config->port_https))) ) {
                return;
            }
            config->hosts[hosts_length*ports+i] = extra+cur;
            cur += n+1; 
        }
    }
}

/**
 * Loads configuration from JSON file
 *
 * @param   config  [config_t *]    "The configuration structure to fill"
 * @param   path    [char *]        "The path to the JSON file"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t config_load(config_t *config, char *path) {
    unsigned int i, j, length;
    char *name;
    json_value *value, *val, *temp;

    json_settings settings = (json_settings){
        .settings = json_enable_comments
    };
    char *error = WSS_malloc(1024*sizeof(char *));

    if ( unlikely(NULL == error) ) {
        return CONFIG_ERROR;
    }

    config->length = strload(path, &config->string);
    if ( unlikely(NULL == config->string) ) {
        return CONFIG_LOAD_ERROR;
    }

    config->data = json_parse_ex(&settings, config->string, config->length, error);

    if ( unlikely(config->data == 0) ) {
        WSS_log(
                SERVER_ERROR,
                error,
                __FILE__,
                __LINE__
               );
        WSS_free((void **) &error);
        return JSON_PARSE_ERROR;
    } else {
        if ( likely(config->data->type == json_object) ) {
            for (i = 0; likely(i < config->data->u.object.length); i++) {
                value = config->data->u.object.values[i].value;
                name = config->data->u.object.values[i].name;

                if ( strncmp(name, "hosts", 5) == 0 ) {
                    if ( value->type == json_array ) {
                        length = value->u.array.length;

                        if (length == 0) {
                            config->hosts_length = length;
                            config->hosts = NULL;
                            continue;
                        }

                        if (NULL == (config->hosts = WSS_calloc(length, sizeof(char *)))) {
                            return MEMORY_ERROR;
                        }

                        config->hosts_length = length;
                        for (j = 0; j < length; j++) {
                            if ( value->u.array.values[j]->type == json_string) {
                                config->hosts[j] =
                                    (char *) value->u.array.values[j]->u.string.ptr;
                            }
                        }
                    }
                } else if ( strncmp(name, "origins", 7) == 0 ) {
                    if ( value->type == json_array ) {
                        length = value->u.array.length;

                        if (length == 0) {
                            config->origins_length = length;
                            config->origins = NULL;
                            continue;
                        }

                        if (NULL == (config->origins = WSS_calloc(length, sizeof(char *)))) {
                            return MEMORY_ERROR;
                        }

                        config->origins_length = length;
                        for (j = 0; j < value->u.array.length; j++) {
                            if ( value->u.array.values[j]->type == json_string) {
                                config->origins[j] =
                                    (char *) value->u.array.values[j]->u.string.ptr;
                            }
                        }
                    }
                } else if ( strncmp(name, "paths", 7) == 0 ) {
                    if ( value->type == json_array ) {
                        length = value->u.array.length;

                        if (length == 0) {
                            config->paths_length = length;
                            config->paths = NULL;
                            continue;
                        }

                        if (NULL == (config->paths = WSS_calloc(length, sizeof(char *)))) {
                            return MEMORY_ERROR;
                        }

                        config->paths_length = length;
                        for (j = 0; j < value->u.array.length; j++) {
                            if ( value->u.array.values[j]->type == json_string) {
                                config->paths[j] =
                                    (char *) value->u.array.values[j]->u.string.ptr;
                            }
                        }
                    }
                } else if ( strncmp(name, "queries", 7) == 0 ) {
                    if ( value->type == json_array ) {
                        length = value->u.array.length;

                        if (length == 0) {
                            config->queries_length = length;
                            config->queries = NULL;
                            continue;
                        }

                        if (NULL == (config->queries = WSS_calloc(length, sizeof(char *)))) {
                            return MEMORY_ERROR;
                        }

                        config->queries_length = length;
                        for (j = 0; j < value->u.array.length; j++) {
                            if ( value->u.array.values[j]->type == json_string) {
                                config->queries[j] =
                                    (char *) value->u.array.values[j]->u.string.ptr;
                            }
                        }
                    }
                } else if ( strncmp(name, "setup", 5) == 0 ) {
                    if ( (val = json_value_find(value, "subprotocols_folder")) != NULL ) {
                        if ( val->type == json_string ) {
                            config->subprotocols_folder = (char *)val->u.string.ptr;
                        }
                    }

                    if ( (val = json_value_find(value, "extensions_folder")) != NULL ) {
                        if ( val->type == json_string ) {
                            config->extensions_folder = (char *)val->u.string.ptr;
                        }
                    }

                    if ( (val = json_value_find(value, "favicon")) != NULL ) {
                        if ( val->type == json_string ) {
                            config->favicon = (char *)val->u.string.ptr;
                        }
                    }

                    if ( (val = json_value_find(value, "timeout")) != NULL ) {
                        if ( val->type == json_integer ) {
                            config->timeout = (unsigned int)val->u.integer;
                        }
                    }

                    if ( (val = json_value_find(value, "ssl")) != NULL ) {
                        if ( val->type == json_object ) {
                            // Getting SSL key path
                            temp = json_value_find(val, "key");
                            if ( temp->type == json_string ) {
                                config->ssl_key = (char *)temp->u.string.ptr;
                            }

                            // Getting SSL cert path
                            temp = json_value_find(val, "cert");
                            if ( temp->type == json_string ) {
                                config->ssl_cert = (char *)temp->u.string.ptr;
                            }

                            // Getting SSL CA cert path
                            temp = json_value_find(val, "ca");
                            if ( temp->type == json_string ) {
                                config->ssl_ca = (char *)temp->u.string.ptr;
                            }
                        }
                    }

                    if ( (val = json_value_find(value, "port")) != NULL ) {
                        if ( val->type == json_object ) {
                            // Getting HTTP port
                            temp = json_value_find(val, "http");
                            if ( temp->type == json_integer ) {
                                config->port_http =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting HTTPS port
                            temp = json_value_find(val, "https");
                            if ( temp->type == json_integer ) {
                                config->port_https =
                                    (unsigned int)temp->u.integer;
                            }
                        }
                    }

                    if ( (val = json_value_find(value, "size")) != NULL ) {
                        if ( val->type == json_object ) {
                            // Getting buffer size
                            temp = json_value_find(val, "buffer");
                            if ( temp->type == json_integer ) {
                                config->size_buffer =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting thread size
                            temp = json_value_find(val, "thread");
                            if ( temp->type == json_integer) {
                                config->size_thread =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting uri size
                            temp = json_value_find(val, "uri");
                            if ( temp->type == json_integer) {
                                config->size_uri =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting header size
                            temp = json_value_find(val, "header");
                            if ( temp->type == json_integer) {
                                config->size_header =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting pipe size
                            temp = json_value_find(val, "pipe");
                            if ( temp->type == json_integer) {
                                config->size_pipe =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting payload size
                            temp = json_value_find(val, "payload");
                            if ( temp->type == json_integer) {
                                config->size_payload =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting message queue size
                            temp = json_value_find(val, "queue");
                            if ( temp->type == json_integer) {
                                config->size_queue =
                                    (unsigned int)temp->u.integer;
                            }
                        }
                    }

                    if ( (val = json_value_find(value, "pool")) != NULL ) {
                        if ( val->type == json_object ) {
                            // Getting pool size
                            temp = json_value_find(val, "size");
                            if ( temp->type == json_integer ) {
                                config->pool_size =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting amount of queues
                            temp = json_value_find(val, "queues");
                            if ( temp->type == json_integer) {
                                config->pool_queues =
                                    (unsigned int)temp->u.integer;
                            }

                            // Getting amount of workers
                            temp = json_value_find(val, "workers");
                            if ( temp->type == json_integer) {
                                config->pool_workers =
                                    (unsigned int)temp->u.integer;
                            }
                        }
                    }
                }
            }
        } else {
            WSS_log(
                    SERVER_TRACE,
                    "Root level of configuration is expected to be a JSON Object.",
                    __FILE__,
                    __LINE__
                   );
            WSS_free((void **) &error);
            return JSON_ROOT_ERROR;
        }
    }

    if ( likely(config->port_http > 0 || config->port_https > 0) ) {
        config_add_port_to_hosts(config);
    }

    WSS_free((void **) &error);

    return SUCCESS;
}

/**
 * Frees allocated memory from configuration
 *
 * @param   config  [config_t *]    "The configuration structure to free"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t config_free(config_t *config) {
    unsigned int amount = 1;

    if ( likely(NULL != config) ) {
        if ( likely(config->port_http > 0) ) {
            amount++;
        }

        if ( likely(config->port_https > 0) ) {
            amount++;
        }

        if ( likely(amount > 1) ) {
            WSS_free((void **)&config->hosts[(config->hosts_length/amount)*(amount-1)]);
        }

        if ( likely(NULL != config->data) ) {
            json_value_free(config->data);
            config->data = NULL;
        }

        WSS_free((void **) &config->string);
        WSS_free((void **) &config->hosts);
        WSS_free((void **) &config->origins);
    }

    return SUCCESS;
}
