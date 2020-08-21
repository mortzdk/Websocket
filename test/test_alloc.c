#include <signal.h>
#include <stddef.h>
#include <criterion/criterion.h>

#include "alloc.h"

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


TestSuite(WSS_malloc, .init = setup, .fini = teardown);

Test(WSS_malloc, size_zero) {
    char *test = (char *)WSS_malloc(0);

    cr_assert(test == NULL); 
}

Test(WSS_malloc, all_bytes_are_zero_bytes) {
    int i, n = 10;
    char *test = (char *)WSS_malloc(n*sizeof(char));

    for (i = 0; i < n; i++) {
        cr_assert(test[i] == '\0'); 
    }

    WSS_free((void **) &test);
    cr_assert(test == NULL);
}

TestSuite(WSS_copy, .init = setup, .fini = teardown);

Test(WSS_copy, size_zero) {
    char *test = NULL;
    char *dup = WSS_copy(test, 0);

    cr_assert(NULL == dup); 
}

Test(WSS_copy, all_bytes_are_zero_bytes) {
    char *test = "TESTING 123";
    char *dup = WSS_copy(test, strlen(test)+1);

    cr_assert(strncmp(test, dup, strlen(test)) == 0); 
    WSS_free((void **) &dup);
}

TestSuite(WSS_calloc, .init = setup, .fini = teardown);

Test(WSS_calloc, memb_zero) {
    char *test = (char *)WSS_calloc(0, sizeof(int));

    cr_assert(test == NULL); 
}

Test(WSS_calloc, size_zero) {
    char *test = (char *)WSS_calloc(10, 0);

    cr_assert(test == NULL); 
}

Test(WSS_calloc, can_use_as_integer_array) {
    int i, n = 10;
    int *test = (int *)WSS_calloc(10, sizeof(int));

    for (i = 0; i < n; i++) {
        cr_assert(test[i] == 0); 
        test[i] = i;
    }

    for (i = 0; i < n; i++) {
        cr_assert(test[i] == i); 
    }

    WSS_free((void **) &test);

    cr_assert(test == NULL);
}

TestSuite(WSS_realloc, .init = setup, .fini = teardown);

Test(WSS_realloc, size_zero) {
    char *test = NULL;
    test = (char *)WSS_realloc((void **)&test, 0, 0);

    cr_assert(test == NULL); 
}

Test(WSS_realloc, realloc_as_malloc) {
    int i, n = 10;
    char *test = (char *) WSS_realloc(NULL, 0, n*sizeof(char));

    for (i = 0; i < n; i++) {
        cr_assert(test[i] == '\0'); 
    }

    WSS_free((void **) &test);

    cr_assert(test == NULL);
}

TestSuite(WSS_realloc_normal, .init = setup, .fini = teardown);

Test(WSS_realloc_normal, realloc_as_malloc) {
    int i, n = 10;
    char *test = (char *) WSS_realloc_normal(NULL, n*sizeof(char));

    for (i = 0; i < n; i++) {
        cr_assert(test[i] == '\0'); 
    }

    WSS_free_normal(test);
}

Test(WSS_realloc, free_ptr_on_size_zero) {
    int i, n = 10;
    char *test = (char *)WSS_malloc(n);
    char *res;

    for (i = 0; i < n; i++) {
        cr_assert(test[i] == '\0'); 
    }

    res = WSS_realloc((void **)&test, n, 0);

    cr_assert(res == NULL); 
    cr_assert(test == NULL); 
}

Test(WSS_realloc, all_bytes_are_zero_bytes) {
    int i, n = 10;
    char *test = (char *)WSS_malloc(n);

    for (i = 0; i < n; i++) {
        cr_assert(test[i] == '\0'); 
    }

    test = WSS_realloc((void **)&test, n, n+5);

    for (i = 0; i < n+5; i++) {
        cr_assert(test[i] == '\0'); 
    }

    WSS_free((void **) &test);

    cr_assert(test == NULL);
}

Test(WSS_realloc, shrinking) {
    int i, n = 10;
    char *test = (char *)WSS_malloc(n);
    char *res;

    for (i = 0; i < n; i++) {
        cr_assert(test[i] == '\0'); 
    }

    res = WSS_realloc((void **)&test, n, n-(n/2));

    cr_assert(test != NULL);
    cr_assert(res != NULL);

    WSS_free((void **) &res);

    cr_assert(res == NULL);
}
