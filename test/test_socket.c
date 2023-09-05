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
    close(server->fd);
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
    close(server->fd);
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
    close(server->fd);
    WSS_free((void **) &server);
}

Test(WSS_socket_bind, invalid_server_fd) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    server->port = 999;
    server->fd = -1;

    cr_assert(WSS_SOCKET_BIND_ERROR == WSS_socket_bind(server));

    // Cleanup
    close(server->fd);
    WSS_free((void **) &server);
}

Test(WSS_socket_bind, bind_socket) {
    wss_server_t *server = (wss_server_t *) WSS_malloc(sizeof(wss_server_t));
    server->port = 4567;

    cr_assert(WSS_SUCCESS == WSS_socket_create(server));
    cr_assert(WSS_SUCCESS == WSS_socket_reuse(server->fd));
    cr_assert(WSS_SUCCESS == WSS_socket_bind(server));

    // Cleanup
    close(server->fd);
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
    close(server->fd);
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
    close(server->fd);
    WSS_free((void **) &server);
}

TestSuite(WSS_socket_threadpool, .init = setup, .fini = teardown);

Test(WSS_socket_threadpool, zero_workers) {
    cr_assert(WSS_THREADPOOL_CREATE_ERROR == WSS_socket_threadpool(0, 0, 0, NULL));
}

Test(WSS_socket_threadpool, zero_queues) {
    cr_assert(WSS_THREADPOOL_CREATE_ERROR == WSS_socket_threadpool(1, 0, 0, NULL));
}

Test(WSS_socket_threadpool, null_pool) {
    cr_assert(WSS_THREADPOOL_CREATE_ERROR == WSS_socket_threadpool(1, 1, 0, NULL));
}

Test(WSS_socket_threadpool, default_stack_size) {
    size_t default_stack_size;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr, &default_stack_size);
    threadpool_t *pool;
    cr_assert(WSS_SUCCESS == WSS_socket_threadpool(1, 1, 0, &pool));
    cr_assert(default_stack_size == threadpool_get_stack_size(pool));
}

Test(WSS_socket_threadpool, stack_size_set) {
    size_t stack_size = 1024*1024*4;
    threadpool_t *pool;
    cr_assert(WSS_SUCCESS == WSS_socket_threadpool(1, 1, stack_size, &pool));
    cr_assert(stack_size == threadpool_get_stack_size(pool));

}
