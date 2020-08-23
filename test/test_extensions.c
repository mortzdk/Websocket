#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <criterion/criterion.h>

#include "alloc.h"
#include "config.h"
#include "extensions.h"

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

#define WSS_EXTENSION_NO_FILE "resources/test-ext-no-file.so"
#define WSS_EXTENSION_NO_ALLOC "resources/test-ext-no-set-allocators.so"
#define WSS_EXTENSION_NO_INIT "resources/test-ext-no-init.so"
#define WSS_EXTENSION_NO_OPEN "resources/test-ext-no-open.so"
#define WSS_EXTENSION_NO_IN_FRAME "resources/test-ext-no-in-frame.so"
#define WSS_EXTENSION_NO_IN_FRAMES "resources/test-ext-no-in-frames.so"
#define WSS_EXTENSION_NO_OUT_FRAME "resources/test-ext-no-out-frame.so"
#define WSS_EXTENSION_NO_OUT_FRAMES "resources/test-ext-no-out-frames.so"
#define WSS_EXTENSION_NO_CLOSE "resources/test-ext-no-close.so"
#define WSS_EXTENSION_NO_DESTROY "resources/test-ext-no-destroy.so"

TestSuite(WSS_load_extensions, .init = setup, .fini = teardown);

Test(WSS_load_extensions, no_config) {
    WSS_load_extensions(NULL);

    cr_assert(NULL == WSS_find_extension(NULL));

    WSS_destroy_extensions();
}

Test(WSS_load_extensions, no_file) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_FILE)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_FILE);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-file"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, no_set_allocators) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_ALLOC)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_ALLOC);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-set-allocators"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, no_init) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_INIT)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_INIT);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-init"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, no_open) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_OPEN)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_OPEN);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-open"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, no_in_frame) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_IN_FRAME)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_IN_FRAME);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-in-frame"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, no_in_frames) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_IN_FRAMES)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_IN_FRAMES);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-in-frames"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}


Test(WSS_load_extensions, no_out_frame) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_OUT_FRAME)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_OUT_FRAME);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-out-frame"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, no_out_frames) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_OUT_FRAMES)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_OUT_FRAMES);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-out-frames"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, no_close) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_CLOSE)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_CLOSE);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-close"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, no_destroy) {
    size_t length = 1;
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    conf->extensions_length = 1;
    conf->extensions = WSS_calloc(length, sizeof(char *));
    conf->extensions[0] = WSS_malloc(sizeof(char)*(strlen(WSS_EXTENSION_NO_DESTROY)+1));
    sprintf(conf->extensions[0], "%s", WSS_EXTENSION_NO_DESTROY);

    conf->extensions_config = WSS_calloc(length, sizeof(char *));
    conf->extensions_config[0] = NULL;

    WSS_load_extensions(conf);

    cr_assert(NULL == WSS_find_extension("test-ext-no-destroy"));

    WSS_destroy_extensions();
    WSS_free((void**) &conf->extensions[0]);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

Test(WSS_load_extensions, successful) {
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));
    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    WSS_load_extensions(conf);

    cr_assert(NULL != WSS_find_extension("permessage-deflate"));

    WSS_destroy_extensions();
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}

