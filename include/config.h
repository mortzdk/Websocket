#ifndef wss_config_h
#define wss_config_h

#include "json.h"
#include "error.h"

typedef struct {
    char *string;
    size_t length;
    char **hosts;
    char **paths;
    char **queries;
    char **origins;
    int hosts_length;
    int paths_length;
    int queries_length;
    int origins_length;
    json_value *data;
    unsigned int port_http;
    unsigned int port_https;
    unsigned int log;
    unsigned int size_uri;
    unsigned int size_payload;
    unsigned int size_header;
    unsigned int size_thread;
    unsigned int size_buffer;
    unsigned int size_queue;
    unsigned int size_ringbuffer;
    unsigned int pool_size;
    unsigned int pool_queues;
    unsigned int pool_workers;
    unsigned int timeout;
    char *ssl_key;
    char *ssl_cert;
    char *ssl_ca_file;
    char *ssl_ca_path;
    char *favicon;
    char **subprotocols;
    unsigned int subprotocols_length;
    char **subprotocols_config;
    char **extensions;
    unsigned int extensions_length;
    char **extensions_config;
} config_t;

/**
 * Loads configuration from JSON file
 *
 * @param   config  [config_t *]    "The configuration structure to fill"
 * @param   path    [char *]        "The path to the JSON file"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t config_load(config_t *config, char *path);

/**
 * Frees allocated memory from configuration
 *
 * @param   config  [config_t *]    "The configuration structure to free"
 * @return 			[wss_error_t]   "The error status"
 */
wss_error_t config_free(config_t *config);

#endif
