#include "core.h"
#include "alloc.h"
#include "memorypool.h"
#include "log.h"

wss_memorypool_t *WSS_memorypool_create(uint64_t block_amount, uint64_t block_size) {
    int err;

    if ( unlikely(block_amount == 0) ) {
        return NULL;
    }

    wss_memorypool_t *pool = (wss_memorypool_t *)WSS_malloc(sizeof(wss_memorypool_t));
    if ( unlikely(pool == NULL) ) {
        return NULL;
    }

    pool->block_amount = block_amount;
    pool->block_size = WSS_MAX(block_size, sizeof(uintptr_t));
    pool->memory = (uintptr_t *)WSS_malloc(pool->block_amount * pool->block_size);
    pool->next = pool->memory;
    pool->blocks_free = block_amount;

    if ( unlikely((err = pthread_mutex_init(&pool->mutex, NULL)) != 0) ) {
        WSS_free((void **) &pool);
        return NULL;
    }

    return pool;
}

uintptr_t * address_from_index(wss_memorypool_t *pool, unsigned int index) {
    return WSS_POINTER_ADD(pool->memory, index * pool->block_size);
}

inline unsigned int index_from_address(wss_memorypool_t *pool, uintptr_t *addr) {
    return (uintptr_t)WSS_POINTER_SUB(addr, pool->memory) / pool->block_size;
}

void *WSS_memorypool_alloc(wss_memorypool_t *pool) {
    uintptr_t *tmp_memory;
    pthread_mutex_lock(&pool->mutex);

    if ( likely(pool->blocks_initialized < pool->block_amount) ) {
        unsigned int *ptr = (unsigned int *) address_from_index(pool, pool->blocks_initialized);
        *ptr = pool->blocks_initialized + 1;
        pool->blocks_initialized += 1;
    }

    if ( unlikely(pool->next == NULL) ) {
        WSS_log_trace("Memorypool is full, allocating more memory");

        tmp_memory = WSS_realloc(
                (void **)&pool->memory, 
                pool->block_amount * pool->block_size, 
                2 * pool->block_amount * pool->block_size
                );
        if ( unlikely(tmp_memory == NULL) ) {
            pthread_mutex_unlock(&pool->mutex);
            return NULL;
        }

        pool->memory = tmp_memory;
        pool->blocks_free = pool->block_amount;
        pool->block_amount = 2*pool->block_amount;
        pool->next = address_from_index(pool, pool->block_amount - pool->blocks_free);
    }

    void* block = pool->next;
    pool->blocks_free -= 1;
    if ( likely(pool->blocks_free > 0) ) {
        pool->next = address_from_index(pool, *((unsigned int *) pool->next));
    } else {
        pool->next = NULL;
    }

    pthread_mutex_unlock(&pool->mutex);

    return block;
}

void WSS_memorypool_dealloc(wss_memorypool_t *pool, void *ptr) {
    pthread_mutex_lock(&pool->mutex);

    if ( unlikely(pool->blocks_free == pool->block_amount) ) {
        pthread_mutex_unlock(&pool->mutex);
        return;
    }

    if ( unlikely(pool->next == NULL) ) {
        *((unsigned int *) ptr) = pool->block_amount;
        pool->next = (uintptr_t *)ptr;
    } else {
        *((unsigned int *) ptr) = index_from_address(pool, pool->next);
        pool->next = (uintptr_t *)ptr;
    }
    pool->blocks_free += 1;

    if ( unlikely(pool->blocks_free == pool->block_amount) ) {
        pthread_cond_signal(&pool->cond);
    }

    pthread_mutex_unlock(&pool->mutex);
}

void WSS_memorypool_destroy(wss_memorypool_t *pool) {
    pthread_mutex_lock(&pool->mutex);

    // Wait for all memory to be released
    while ( likely(pool->blocks_free != pool->block_amount) ) {
        pthread_cond_wait(&pool->cond, &pool->mutex);
    }

    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);

    WSS_free((void **)&pool->memory);
    WSS_free((void **)&pool);
}
