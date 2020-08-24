#include <criterion/criterion.h>

#include "alloc.h"
#include "server.h"
#include "http.h"
#include "socket.h"
#include "rpmalloc.h"

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

TestSuite(WSS_socket_create, .init = setup, .fini = teardown);

Test(WSS_socket_create, null_server) {
    cr_assert(WSS_SOCKET_CREATE_ERROR == WSS_socket_create(NULL));
}

Test(WSS_socket_create, creating_socket) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));

    cr_assert(WSS_SUCCESS == WSS_socket_create(server));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
}

TestSuite(WSS_socket_reuse, .init = setup, .fini = teardown);

Test(WSS_socket_reuse, invalid_fd) {
    cr_assert(WSS_SOCKET_REUSE_ERROR == WSS_socket_reuse(-1));
}

Test(WSS_socket_reuse, reuse_socket) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));

    cr_assert(WSS_SUCCESS == WSS_socket_create(server));
    cr_assert(WSS_SUCCESS == WSS_socket_reuse(server->fd));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
}

TestSuite(WSS_socket_bind, .init = setup, .fini = teardown);

Test(WSS_socket_bind, null_server) {
    cr_assert(WSS_SOCKET_BIND_ERROR == WSS_socket_bind(NULL));
}

Test(WSS_socket_bind, invalid_server_port) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    server->port = -1;

    cr_assert(WSS_SOCKET_BIND_ERROR == WSS_socket_bind(server));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
}

Test(WSS_socket_bind, invalid_server_fd) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    server->port = 999;
    server->fd = -1;

    cr_assert(WSS_SOCKET_BIND_ERROR == WSS_socket_bind(server));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
}

Test(WSS_socket_bind, bind_socket) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    server->port = 4567;

    cr_assert(WSS_SUCCESS == WSS_socket_create(server));
    cr_assert(WSS_SUCCESS == WSS_socket_reuse(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_bind(server));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
}

TestSuite(WSS_socket_non_blocking, .init = setup, .fini = teardown);

Test(WSS_socket_non_blocking, invalid_fd) {
    cr_assert(WSS_SOCKET_NONBLOCKED_ERROR == WSS_socket_non_blocking(-1));
}

Test(WSS_socket_non_blocking, non_blocking_socket) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    server->port = 4567;

    cr_assert(WSS_SUCCESS == WSS_socket_create(server));
    cr_assert(WSS_SUCCESS == WSS_socket_reuse(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_bind(server));
    cr_assert(WSS_SUCCESS == WSS_socket_non_blocking(server->fd));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
}

TestSuite(WSS_socket_listen, .init = setup, .fini = teardown);

Test(WSS_socket_listen, invalid_fd) {
    cr_assert(WSS_SOCKET_LISTEN_ERROR == WSS_socket_listen(-1));
}

Test(WSS_socket_listen, listen_socket) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    server->port = 4567;

    cr_assert(WSS_SUCCESS == WSS_socket_create(server));
    cr_assert(WSS_SUCCESS == WSS_socket_reuse(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_bind(server));
    cr_assert(WSS_SUCCESS == WSS_socket_non_blocking(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_listen(server->fd));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
}

TestSuite(WSS_socket_threadpool, .init = setup, .fini = teardown);

Test(WSS_socket_threadpool, null_server) {
    cr_assert(WSS_THREADPOOL_CREATE_ERROR == WSS_socket_threadpool(NULL));
}

Test(WSS_socket_threadpool, null_config) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    cr_assert(WSS_THREADPOOL_CREATE_ERROR == WSS_socket_threadpool(server));
}

Test(WSS_socket_threadpool, invalid_create_threadpool_params) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    server->port = conf->port_http;
    server->config = conf;

    cr_assert(WSS_SUCCESS == WSS_socket_create(server));
    cr_assert(WSS_SUCCESS == WSS_socket_reuse(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_bind(server));
    cr_assert(WSS_SUCCESS == WSS_socket_non_blocking(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_listen(server->fd));
    cr_assert(WSS_THREADPOOL_CREATE_ERROR == WSS_socket_threadpool(server));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
    WSS_free((void**) &conf);
}

Test(WSS_socket_threadpool, threadpool_socket) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    wss_config_t *conf = (wss_config_t *) WSS_malloc(sizeof(wss_config_t));

    cr_assert(WSS_SUCCESS == WSS_config_load(conf, "resources/test_wss.json"));

    server->port = conf->port_http;
    server->config = conf;

    cr_assert(WSS_SUCCESS == WSS_socket_create(server));
    cr_assert(WSS_SUCCESS == WSS_socket_reuse(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_bind(server));
    cr_assert(WSS_SUCCESS == WSS_socket_non_blocking(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_listen(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_threadpool(server));

    // Cleanup
    WSS_http_server_free(server);
    pthread_mutex_destroy(&server->lock);
    WSS_free((void **) &server);
    WSS_config_free(conf);
    WSS_free((void**) &conf);
}
