#include <signal.h>
#include <stddef.h>
#include <criterion/criterion.h>

#include "alloc.h"
#include "config.h"
#include "error.h"
#include "str.h"

static void setup(void) {
#ifdef USE_RPMALLOC
    rpmalloc_initialize();
#endif
}

static void teardown(void) {
#ifdef USE_RPMALLOC
    rpmalloc_finalize();
#endif
}

TestSuite(WSS_config_load, .init = setup, .fini = teardown);

Test(WSS_config_load, none_existing_file) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_LOAD_ERROR == WSS_config_load(conf, "resources/none_existing.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, no_starting_object) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_JSON_ROOT_ERROR == WSS_config_load(conf, "resources/test_no_starting_object_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_JSON_PARSE_ERROR == WSS_config_load(conf, "resources/test_invalid_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_host_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_INVALID_HOST == WSS_config_load(conf, "resources/test_invalid_host_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_origin_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_INVALID_ORIGIN == WSS_config_load(conf, "resources/test_invalid_origin_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_path_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_INVALID_PATH == WSS_config_load(conf, "resources/test_invalid_path_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_query_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_INVALID_QUERY == WSS_config_load(conf, "resources/test_invalid_query_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_subprotocol_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_INVALID_SUBPROTOCOL == WSS_config_load(conf, "resources/test_invalid_subprotocol_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_subprotocol_config2) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_INVALID_SUBPROTOCOL == WSS_config_load(conf, "resources/test_invalid_subprotocol2_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_extension_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_INVALID_EXTENSION == WSS_config_load(conf, "resources/test_invalid_extension_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, invalid_extension_config2) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_INVALID_EXTENSION == WSS_config_load(conf, "resources/test_invalid_extension2_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, subprotocol_and_extension_with_invalid_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_subprotocol_and_extension_with_invalid_config_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, no_port_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_NO_PORT_ERROR == WSS_config_load(conf, "resources/test_no_port_wss.json"));

    WSS_free((void**) &conf);
}

Test(WSS_config_load, valid_config_empty_arrays) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss_empty_arrays.json"));

    // Favicon
    cr_expect(strncmp(conf->favicon, "favicon.ico", 11) == 0); 

    // Check SSL
    cr_expect(strncmp(conf->ssl_ca_file, "root.pem", 8) == 0); 
    cr_expect(strncmp(conf->ssl_key, "key.pem", 7) == 0); 
    cr_expect(strncmp(conf->ssl_cert, "cert.pem", 8) == 0); 
    cr_expect(strncmp(conf->ssl_ca_path, "/usr/lib/ssl/certs/", 19) == 0); 
    cr_expect(strncmp(conf->ssl_dhparam, "dhparam.pem", 11) == 0); 
    cr_expect(strncmp(conf->ssl_cipher_list, "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-SHA:ECDHE-ECDSA-AES256-SHA:ECDHE-ECDSA-AES128-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA256", 437) == 0); 
    cr_expect(strncmp(conf->ssl_cipher_suites, "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_8_SHA256:TLS_AES_128_CCM_SHA256", 122) == 0); 
    cr_expect(conf->ssl_compression == false); 
    cr_expect(conf->ssl_peer_cert == false); 

    // Log
    cr_expect(conf->log == 7); 

    // Hosts
    cr_expect(conf->hosts == NULL);
    cr_expect(conf->hosts_length == 0);

    // Origins
    cr_expect(conf->origins == NULL);
    cr_expect(conf->origins_length == 0);

    // Paths
    cr_expect(conf->paths == NULL);
    cr_expect(conf->paths_length == 0);

    // Query parameters
    cr_expect(conf->queries == NULL);
    cr_expect(conf->queries_length == 0);

    // Timeouts
    cr_expect(conf->timeout_poll == -1); 
    cr_expect(conf->timeout_read == -1); 
    cr_expect(conf->timeout_write == -1); 
    cr_expect(conf->timeout_client == -1); 
    cr_expect(conf->timeout_pings == 0); 

    // Ports
    cr_expect(conf->port_http == 9010); 
    cr_expect(conf->port_https == 9011); 

    // Sizes
    cr_expect(conf->size_uri == 128); 
    cr_expect(conf->size_ringbuffer == 128); 
    cr_expect(conf->size_buffer == 25600); 
    cr_expect(conf->size_header == 1024); 
    cr_expect(conf->size_thread == 524288); 
    cr_expect(conf->size_frame == 16777215); 
    cr_expect(conf->size_payload == 1024); 
    cr_expect(conf->max_frames == 1048576); 

    // Pool
    cr_expect(conf->pool_io_workers == 6); 
    cr_expect(conf->pool_io_tasks == 32768); 
    cr_expect(conf->pool_connect_workers == 2); 
    cr_expect(conf->pool_connect_tasks == 32768); 

    // Subprotocols
    cr_expect(conf->subprotocols_default == 0); 
    cr_expect(conf->subprotocols_length == 0); 
    cr_expect(conf->subprotocols == NULL);

    // Extensions
    cr_expect(conf->extensions_length == 0); 
    cr_expect(conf->extensions_config == NULL);
    cr_expect(conf->extensions == NULL);

    cr_assert(WSS_config_free(conf) == WSS_SUCCESS); 
    WSS_free((void**) &conf);
}

Test(WSS_config_load, valid_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    // Favicon
    cr_expect(strncmp(conf->favicon, "favicon.ico", 11) == 0); 

    // Check SSL
    cr_expect(strncmp(conf->ssl_ca_file, "root.pem", 8) == 0); 
    cr_expect(strncmp(conf->ssl_key, "key.pem", 7) == 0); 
    cr_expect(strncmp(conf->ssl_cert, "cert.pem", 8) == 0); 
    cr_expect(strncmp(conf->ssl_ca_path, "/usr/lib/ssl/certs/", 19) == 0); 
    cr_expect(strncmp(conf->ssl_dhparam, "dhparam.pem", 11) == 0); 
    cr_expect(strncmp(conf->ssl_cipher_list, "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-SHA:ECDHE-ECDSA-AES256-SHA:ECDHE-ECDSA-AES128-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA256", 437) == 0); 
    cr_expect(strncmp(conf->ssl_cipher_suites, "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_8_SHA256:TLS_AES_128_CCM_SHA256", 122) == 0); 
    cr_expect(conf->ssl_compression == false); 
    cr_expect(conf->ssl_peer_cert == false); 

    // Log
    cr_expect(conf->log == 7); 

    // Hosts
    cr_expect(strinarray("localhost", (const char **)conf->hosts, conf->hosts_length) == 0);
    cr_expect(strinarray("127.0.0.1", (const char **)conf->hosts, conf->hosts_length) == 0);

    // Origins
    cr_expect(strinarray("localhost", (const char **)conf->origins, conf->origins_length) == 0);
    cr_expect(strinarray("127.0.0.1", (const char **)conf->origins, conf->origins_length) == 0);

    // Paths
    cr_expect(strinarray("test/path", (const char **)conf->paths, conf->paths_length) == 0);
    cr_expect(strinarray("another/test/path", (const char **)conf->paths, conf->paths_length) == 0);

    // Query parameters
    cr_expect(strinarray("csrf_token=[^&]*", (const char **)conf->queries, conf->queries_length) == 0);
    cr_expect(strinarray("access_token=[^&]*", (const char **)conf->queries, conf->queries_length) == 0);

    // Timeouts
    cr_expect(conf->timeout_poll == 10); 
    cr_expect(conf->timeout_read == 10); 
    cr_expect(conf->timeout_write == 10); 
    cr_expect(conf->timeout_client == 600); 
    cr_expect(conf->timeout_pings == 1); 

    // Ports
    cr_expect(conf->port_http == 9010); 
    cr_expect(conf->port_https == 9011); 

    // Sizes
    cr_expect(conf->size_uri == 128); 
    cr_expect(conf->size_ringbuffer == 128); 
    cr_expect(conf->size_buffer == 25600); 
    cr_expect(conf->size_header == 1024); 
    cr_expect(conf->size_thread == 524288); 
    cr_expect(conf->size_frame == 128); 
    cr_expect(conf->size_payload == 1024); 
    cr_expect(conf->max_frames == 1048576); 

    // Pool
    cr_expect(conf->pool_io_workers == 6); 
    cr_expect(conf->pool_io_tasks == 32768); 
    cr_expect(conf->pool_connect_workers == 2); 
    cr_expect(conf->pool_connect_tasks == 32768); 

    // Subprotocols
    cr_expect(conf->subprotocols_default == 1); 
    cr_expect(conf->subprotocols_length == 2); 
    cr_expect(strinarray("subprotocols/echo/echo.so", (const char **)conf->subprotocols, conf->subprotocols_length) == 0);
    cr_expect(strinarray("subprotocols/broadcast/broadcast.so", (const char **)conf->subprotocols, conf->subprotocols_length) == 0);

    // Extensions
    cr_expect(conf->extensions_length == 1); 
    cr_expect(strinarray("extensions/permessage-deflate/permessage-deflate.so", (const char **)conf->extensions, conf->extensions_length) == 0);
    cr_expect(strinarray("server_max_window_bits=10;client_max_window_bits=10;memory_level=8", (const char **)conf->extensions_config, conf->extensions_length) == 0);

    cr_assert(WSS_config_free(conf) == WSS_SUCCESS); 
    WSS_free((void**) &conf);
}

TestSuite(WSS_config_free, .init = setup, .fini = teardown);

Test(WSS_config_free, size_zero) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    WSS_config_load(conf, "resources/test_wss.json");

    cr_assert(WSS_config_free(conf) == WSS_SUCCESS); 

    cr_expect(conf->data == NULL); 
    cr_expect(conf->hosts == NULL); 
    cr_expect(conf->origins == NULL); 

    WSS_free((void**) &conf);
}
