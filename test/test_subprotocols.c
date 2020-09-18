#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <criterion/criterion.h>

#include "alloc.h"
#include "config.h"
#include "subprotocols.h"

#define WSS_SUBPROTOCOL_NO_FILE "resources/test-sub-no-file.so"
#define WSS_SUBPROTOCOL_NO_SET_ALLOCATORS "resources/test-sub-no-set-allocators.so"
#define WSS_SUBPROTOCOL_NO_INIT "resources/test-sub-no-init.so"
#define WSS_SUBPROTOCOL_NO_CONNECT "resources/test-sub-no-connect.so"
#define WSS_SUBPROTOCOL_NO_MESSAGE "resources/test-sub-no-message.so"
#define WSS_SUBPROTOCOL_NO_WRITE "resources/test-sub-no-write.so"
#define WSS_SUBPROTOCOL_NO_CLOSE "resources/test-sub-no-close.so"
#define WSS_SUBPROTOCOL_NO_DESTROY "resources/test-sub-no-destroy.so"

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

TestSuite(WSS_load_subprotocols, .init = setup, .fini = teardown);

Test(WSS_load_subprotocols, no_config) {
    WSS_load_subprotocols(NULL);

    cr_assert(NULL == WSS_find_subprotocol(NULL));

    WSS_destroy_subprotocols();
}

Test(WSS_load_subprotocols, no_file) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->subprotocols_length = 1;
    conf->subprotocols = WSS_calloc(length, sizeof(char *));
    conf->subprotocols[0] = WSS_malloc(sizeof(char)*(strlen(WSS_SUBPROTOCOL_NO_FILE)+1));
    sprintf(conf->subprotocols[0], "%s", WSS_SUBPROTOCOL_NO_FILE);

    conf->subprotocols_config = WSS_calloc(length, sizeof(char *));
    conf->subprotocols_config[0] = NULL;

    WSS_load_subprotocols(conf);

    cr_assert(NULL == WSS_find_subprotocol("test-sub-no-file"));

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf->subprotocols[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_subprotocols, no_set_allocators) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->subprotocols_length = 1;
    conf->subprotocols = WSS_calloc(length, sizeof(char *));
    conf->subprotocols[0] = WSS_malloc(sizeof(char)*(strlen(WSS_SUBPROTOCOL_NO_SET_ALLOCATORS)+1));
    sprintf(conf->subprotocols[0], "%s", WSS_SUBPROTOCOL_NO_SET_ALLOCATORS);

    conf->subprotocols_config = WSS_calloc(length, sizeof(char *));
    conf->subprotocols_config[0] = NULL;

    WSS_load_subprotocols(conf);

    cr_assert(NULL == WSS_find_subprotocol("test-sub-no-set-allocators"));

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf->subprotocols[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_subprotocols, no_init) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->subprotocols_length = 1;
    conf->subprotocols = WSS_calloc(length, sizeof(char *));
    conf->subprotocols[0] = WSS_malloc(sizeof(char)*(strlen(WSS_SUBPROTOCOL_NO_INIT)+1));
    sprintf(conf->subprotocols[0], "%s", WSS_SUBPROTOCOL_NO_INIT);

    conf->subprotocols_config = WSS_calloc(length, sizeof(char *));
    conf->subprotocols_config[0] = NULL;

    WSS_load_subprotocols(conf);

    cr_assert(NULL == WSS_find_subprotocol("test-sub-no-init"));

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf->subprotocols[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_subprotocols, no_connect) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->subprotocols_length = 1;
    conf->subprotocols = WSS_calloc(length, sizeof(char *));
    conf->subprotocols[0] = WSS_malloc(sizeof(char)*(strlen(WSS_SUBPROTOCOL_NO_CONNECT)+1));
    sprintf(conf->subprotocols[0], "%s", WSS_SUBPROTOCOL_NO_CONNECT);

    conf->subprotocols_config = WSS_calloc(length, sizeof(char *));
    conf->subprotocols_config[0] = NULL;

    WSS_load_subprotocols(conf);

    cr_assert(NULL == WSS_find_subprotocol("test-sub-no-connect"));

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf->subprotocols[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_subprotocols, no_message) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->subprotocols_length = 1;
    conf->subprotocols = WSS_calloc(length, sizeof(char *));
    conf->subprotocols[0] = WSS_malloc(sizeof(char)*(strlen(WSS_SUBPROTOCOL_NO_MESSAGE)+1));
    sprintf(conf->subprotocols[0], "%s", WSS_SUBPROTOCOL_NO_MESSAGE);

    conf->subprotocols_config = WSS_calloc(length, sizeof(char *));
    conf->subprotocols_config[0] = NULL;

    WSS_load_subprotocols(conf);

    cr_assert(NULL == WSS_find_subprotocol("test-sub-no-message"));

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf->subprotocols[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_subprotocols, no_write) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->subprotocols_length = 1;
    conf->subprotocols = WSS_calloc(length, sizeof(char *));
    conf->subprotocols[0] = WSS_malloc(sizeof(char)*(strlen(WSS_SUBPROTOCOL_NO_WRITE)+1));
    sprintf(conf->subprotocols[0], "%s", WSS_SUBPROTOCOL_NO_WRITE);

    conf->subprotocols_config = WSS_calloc(length, sizeof(char *));
    conf->subprotocols_config[0] = NULL;

    WSS_load_subprotocols(conf);

    cr_assert(NULL == WSS_find_subprotocol("test-sub-no-write"));

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf->subprotocols[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_subprotocols, no_close) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->subprotocols_length = 1;
    conf->subprotocols = WSS_calloc(length, sizeof(char *));
    conf->subprotocols[0] = WSS_malloc(sizeof(char)*(strlen(WSS_SUBPROTOCOL_NO_CLOSE)+1));
    sprintf(conf->subprotocols[0], "%s", WSS_SUBPROTOCOL_NO_CLOSE);

    conf->subprotocols_config = WSS_calloc(length, sizeof(char *));
    conf->subprotocols_config[0] = NULL;

    WSS_load_subprotocols(conf);

    cr_assert(NULL == WSS_find_subprotocol("test-sub-no-close"));

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf->subprotocols[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_subprotocols, no_destroy) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->subprotocols_length = 1;
    conf->subprotocols = WSS_calloc(length, sizeof(char *));
    conf->subprotocols[0] = WSS_malloc(sizeof(char)*(strlen(WSS_SUBPROTOCOL_NO_DESTROY)+1));
    sprintf(conf->subprotocols[0], "%s", WSS_SUBPROTOCOL_NO_DESTROY);

    conf->subprotocols_config = WSS_calloc(length, sizeof(char *));
    conf->subprotocols_config[0] = NULL;

    WSS_load_subprotocols(conf);

    cr_assert(NULL == WSS_find_subprotocol("test-sub-no-destroy"));

    WSS_destroy_subprotocols();
    WSS_free((void**) &conf->subprotocols[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_subprotocols, successful) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_subprotocols(conf);

    cr_assert(NULL != WSS_find_subprotocol("echo"));
    cr_assert(NULL != WSS_find_subprotocol("broadcast"));

    WSS_destroy_subprotocols();
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

