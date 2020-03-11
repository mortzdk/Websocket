#include <signal.h>
#include <stddef.h>
#include <criterion/criterion.h>

#include "alloc.h"
#include "config.h"
#include "error.h"
#include "str.h"

Test(config_init, none_existing_file) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_LOAD_ERROR == WSS_config_load(conf, "resources/none_existing.json"));

    WSS_free((void**) &conf);
}

Test(config_init, invalid_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_CONFIG_JSON_PARSE_ERROR == WSS_config_load(conf, "resources/test_invalid_wss.json"));

    WSS_free((void**) &conf);
}

Test(config_init, valid_config) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    cr_expect(strncmp(conf->favicon, "favicon.ico", 11) == 0); 
    cr_expect(strncmp(conf->ssl_ca_file, "root.pem", 8) == 0); 
    cr_expect(strncmp(conf->ssl_key, "key.pem", 7) == 0); 
    cr_expect(strncmp(conf->ssl_cert, "cert.pem", 8) == 0); 

    cr_expect(strinarray("localhost", (const char **)conf->hosts, conf->hosts_length) == 0);
    cr_expect(strinarray("127.0.0.1", (const char **)conf->hosts, conf->hosts_length) == 0);

    cr_expect(strinarray("localhost", (const char **)conf->origins, conf->origins_length) == 0);
    cr_expect(strinarray("127.0.0.1", (const char **)conf->origins, conf->origins_length) == 0);

    cr_expect(conf->timeout == 1000); 
    cr_expect(conf->port_http == 9010); 
    cr_expect(conf->port_https == 9011); 
    cr_expect(conf->size_uri == 8192); 
    cr_expect(conf->size_ringbuffer == 128); 
    cr_expect(conf->size_buffer == 25600); 
    cr_expect(conf->size_header == 8192); 
    cr_expect(conf->size_thread == 524288); 

    cr_expect(conf->pool_workers == 4); 

    WSS_free((void**) &conf);
}

Test(config_free, size_zero) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    WSS_config_load(conf, "resources/test_wss.json");

    cr_assert(WSS_config_free(conf) == WSS_SUCCESS); 

    cr_expect(conf->data == NULL); 
    cr_expect(conf->hosts == NULL); 
    cr_expect(conf->origins == NULL); 

    WSS_free((void**) &conf);
}
