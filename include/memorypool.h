#pragma once

#ifndef WSS_MEMORYPOOL_H
#define WSS_MEMORYPOOL_H

#include <pthread.h>
#include <stdint.h>

typedef struct {
    uint64_t block_amount;
    uint64_t block_size;
    uint64_t blocks_free;
    uint64_t blocks_initialized;
    uintptr_t *memory;
    uintptr_t *next;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} wss_memorypool_t;

wss_memorypool_t *WSS_memorypool_create(uint64_t block_amount, uint64_t block_size);
void *WSS_memorypool_alloc(wss_memorypool_t *pool);
void WSS_memorypool_dealloc(wss_memorypool_t *pool, void *ptr);
void WSS_memorypool_destroy(wss_memorypool_t *pool);
#endif
