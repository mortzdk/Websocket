#include <signal.h>
#include <stddef.h>
#include <criterion/criterion.h>

#include "alloc.h"

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
}

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
}

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
}
