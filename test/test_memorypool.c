#include <signal.h>
#include <stddef.h>
#include <criterion/criterion.h>

#include "alloc.h"
#include "core.h"
#include "memorypool.h"

uint64_t block_amount = 20;

typedef struct {
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
    uint64_t e;
} wss_memorypool_test_t;

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

TestSuite(WSS_memorypool_create, .init = setup, .fini = teardown);

Test(WSS_memorypool_create, block_amount_zero) {
    wss_memorypool_t *p = WSS_memorypool_create(0, 0);
    cr_assert(p == NULL); 
}

Test(WSS_memorypool_create, block_size_below_pointer_size) {
    wss_memorypool_t *p = WSS_memorypool_create(block_amount, sizeof(uintptr_t)-1);

    cr_assert(p != NULL); 
    cr_assert(p->block_amount == block_amount); 
    cr_assert(p->block_size == sizeof(uintptr_t)); 
    cr_assert(p->memory != NULL); 
    cr_assert(p->next == p->memory); 
    cr_assert(p->blocks_free == block_amount); 

    WSS_memorypool_destroy(p);
}

Test(WSS_memorypool_create, block_size) {
    wss_memorypool_t *p = WSS_memorypool_create(block_amount, sizeof(wss_memorypool_test_t));

    cr_assert(p != NULL); 
    cr_assert(p->block_amount == block_amount); 
    cr_assert(p->block_size == sizeof(wss_memorypool_test_t)); 
    cr_assert(p->memory != NULL); 
    cr_assert(p->next == p->memory); 
    cr_assert(p->blocks_free == block_amount); 

    WSS_memorypool_destroy(p);
}

TestSuite(wss_memorypool_alloc, .init = setup, .fini = teardown);

Test(wss_memorypool_alloc, alloc_without_expand) {
    wss_memorypool_t *p = WSS_memorypool_create(block_amount, sizeof(wss_memorypool_test_t));
    wss_memorypool_test_t *tests[block_amount];
    for (uint64_t i = 0; i < block_amount; i += 1) {
        cr_assert((tests[i] = WSS_memorypool_alloc(p)) != NULL);
    }

    for (uint64_t i = 0; i < block_amount; i += 1) {
        WSS_memorypool_dealloc(p, tests[i]);
    }

    WSS_memorypool_destroy(p);
}

Test(wss_memorypool_alloc, alloc_with_expand) {
    wss_memorypool_t *p = WSS_memorypool_create(block_amount, sizeof(wss_memorypool_test_t));
    wss_memorypool_test_t *tests[block_amount*2+1];
    for (uint64_t j = 0; j < 2; j += 1) {
        for (uint64_t i = j; i <= block_amount; i += 1) {
            cr_assert((tests[i+(j*block_amount)] = WSS_memorypool_alloc(p)) != NULL);
        }
        cr_assert(p->block_amount == (2 << j) * block_amount); 
        cr_assert(p->blocks_free == (1 << j) * block_amount - 1); 
    }

    for (uint64_t i = 0; i < block_amount*2+1; i += 1) {
        WSS_memorypool_dealloc(p, tests[i]);
    }

    WSS_memorypool_destroy(p);
}

Test(wss_memorypool_alloc, alloc_dealloc) {
    wss_memorypool_t *p = WSS_memorypool_create(block_amount, sizeof(wss_memorypool_test_t));
    wss_memorypool_test_t *tests[block_amount];
    for (uint64_t i = 0; i < block_amount-1; i += 1) {
        tests[i] = (wss_memorypool_test_t *) WSS_memorypool_alloc(p);
        cr_assert(tests[i] != NULL);
        tests[i]->e = i;
    }

    for (int64_t i = block_amount-1; i >= 0; i -= 1) {
        if (i%2 == 0) {
            WSS_memorypool_dealloc(p, tests[i]);
            cr_assert((uintptr_t *)tests[i] == p->next);
            cr_assert(WSS_POINTER_ADD(p->memory, i * p->block_size) == p->next);
        }
    }

    for (uint64_t i = 0; i < block_amount; i += 1) {
        if (i%2 == 0) {
            tests[i] = (wss_memorypool_test_t *) WSS_memorypool_alloc(p);
            cr_assert(tests[i] != NULL);
            cr_assert(tests[i]->e == (uint64_t)i);
        }
    }

    cr_assert(p->block_amount == block_amount); 
    cr_assert(p->blocks_free == 1); 

    for (uint64_t i = 0; i < block_amount; i += 1) {
        WSS_memorypool_dealloc(p, tests[i]);
    }

    WSS_memorypool_destroy(p);
}

TestSuite(WSS_memorypool_dealloc, .init = setup, .fini = teardown);

Test(WSS_memorypool_dealloc, dealloc) {
    wss_memorypool_t *p = WSS_memorypool_create(block_amount, sizeof(wss_memorypool_test_t));
    wss_memorypool_test_t *tests[block_amount*2];
    for (uint64_t j = 0; j < 2; j += 1) {
        for (uint64_t i = 0; i < block_amount; i += 1) {
            tests[i*(j+1)] = (wss_memorypool_test_t *) WSS_memorypool_alloc(p);
            cr_assert(tests[i*(j+1)] != NULL);
        }
    }

    for (uint64_t j = 0; j < 2; j += 1) {
        for (uint64_t i = 0; i < block_amount; i += 1) {
            WSS_memorypool_dealloc(p, tests[i*(j+1)]);
            cr_assert((uintptr_t *)tests[i*(j+1)] == p->next);
        }
    }
    WSS_memorypool_destroy(p);
}
