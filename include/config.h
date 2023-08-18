#pragma once 

#ifndef wss_config_h
#define wss_config_h

#include <stdbool.h>

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
    unsigned int size_ringbuffer;
    unsigned int size_frame;
    unsigned int pool_connect_tasks;
    unsigned int pool_connect_workers;
    unsigned int pool_io_tasks;
    unsigned int pool_io_workers;
    unsigned int max_frames;
    unsigned int timeout_pings;
    int timeout_poll;
    int timeout_read;
    int timeout_write;
    long int timeout_client;
    char *ssl_key;
    char *ssl_cert;
    char *ssl_ca_file;
    char *ssl_ca_path;
    char *ssl_cipher_list;
    char *ssl_cipher_suites;
    char *ssl_dhparam;
    bool ssl_compression;
    bool ssl_peer_cert;
    char *favicon;
    unsigned int subprotocols_default;
    char **subprotocols;
    unsigned int subprotocols_length;
    char **subprotocols_config;
    char **extensions;
    unsigned int extensions_length;
    char **extensions_config;
} wss_config_t;

/**
 * Loads configuration from JSON file
 *
 * @param   config  [wss_config_t *] "The configuration structure to fill"
 * @param   path    [char *]         "The path to the JSON file"
 * @return 			[wss_error_t]    "The error status"
 */
wss_error_t WSS_config_load(wss_config_t *config, char *path);

/**
 * Frees allocated memory from configuration
 *
 * @param   config  [wss_config_t *] "The configuration structure to free"
 * @return 			[wss_error_t]    "The error status"
 */
wss_error_t WSS_config_free(wss_config_t *config);

#endif
