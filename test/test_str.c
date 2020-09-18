#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <criterion/criterion.h>

#include "str.h"
#include "sha1.h"
#include "alloc.h"

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#else
#define SHA_DIGEST_LENGTH 20
#include "sha1.h"
#endif

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

TestSuite(bin2hex, .init = setup, .fini = teardown);

Test(bin2hex, zero_length) {
    char *string = "";
    char *str = bin2hex((const char unsigned *) string, 0);

    cr_assert(str != NULL);
    cr_assert(str[0] == '\0'); 

    WSS_free((void**)&str);
}

Test(bin2hex, simple_string) {
    char *str;
    const char unsigned *string = (const char unsigned *)WSS_malloc(6);
    sprintf((char *)string, "%s", "12345");

    str = bin2hex(string, strlen((char *)string));

    cr_assert(str != NULL);
    cr_assert(strncmp(str, "3132333435", 10) == 0); 

    WSS_free((void **)&str);
    WSS_free((void **) &string);
}

Test(bin2hex, sha1_string) {
    char *str;
    char sha1Key[SHA_DIGEST_LENGTH];
    const char unsigned *string = (const char unsigned *)WSS_malloc(6);
    sprintf((char *)string, "%s", "12345");

#ifdef USE_OPENSSL
    SHA1((const unsigned char *)string, strlen((char *)string), (unsigned char*) sha1Key);
#else
    SHA1Context sha;
    int i, b;
    memset(sha1Key, '\0', SHA_DIGEST_LENGTH);

    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char*) string, strlen((char *)string));
    if ( SHA1Result(&sha) ) {
        for (i = 0; i < 5; i++) {
            b = htonl(sha.Message_Digest[i]);
            memcpy(sha1Key+(4*i), (unsigned char *) &b, 4);
        }
    }
#endif
    str = bin2hex((const unsigned char *)sha1Key, SHA_DIGEST_LENGTH);

    cr_assert(str != NULL);
    cr_assert(strncmp(str, "8cb2237d0679ca88db6464eac60da96345513964", SHA_DIGEST_LENGTH));

    WSS_free((void **) &str);
    WSS_free((void **) &string);
}

TestSuite(strinarray, .init = setup, .fini = teardown);

Test(strinarray, test_zero_length) {
    int i;
    int length = 0;
    char **array = (char **) WSS_calloc(100, sizeof(char *));

    for (i = 0; i < 100; i++) {
        array[i] = WSS_malloc(3);
        sprintf(array[i], "%d", i);
    }

    cr_assert(strinarray("87", (const char **)array, length) == -1);
    cr_assert(strinarray("34", (const char **)array, length) == -1);
    cr_assert(strinarray("0", (const char **)array, length) == -1);
    cr_assert(strinarray("99", (const char **)array, length) == -1);
    cr_assert(strinarray("37", (const char **)array, length) == -1);
    cr_assert(strinarray("100", (const char **)array, length) == -1);
    cr_assert(strinarray("testing123", (const char **)array, length) == -1);
    cr_assert(strinarray("lol", (const char **)array, length) == -1);

    for (i = 0; i < 100; i++) {
        WSS_free((void **)&array[i]);
    }
    WSS_free((void **)&array);
}

Test(strinarray, test_string_in_first_half_array) {
    int i;
    int length = 100;
    char **array = (char **) WSS_calloc(length, sizeof(char *));

    for (i = 0; i < length; i++) {
        array[i] = WSS_malloc(3);
        sprintf(array[i], "%d", i);
    }

    cr_assert(strinarray("87", (const char **)array, length/2) == -1);
    cr_assert(strinarray("34", (const char **)array, length/2) == 0);
    cr_assert(strinarray("0", (const char **)array, length/2) == 0);
    cr_assert(strinarray("99", (const char **)array, length/2) == -1);
    cr_assert(strinarray("37", (const char **)array, length/2) == 0);
    cr_assert(strinarray("100", (const char **)array, length/2) == -1);
    cr_assert(strinarray("50", (const char **)array, length/2) == -1);
    cr_assert(strinarray("49", (const char **)array, length/2) == 0);
    cr_assert(strinarray("testing123", (const char **)array, length/2) == -1);
    cr_assert(strinarray("lol", (const char **)array, length/2) == -1);

    for (i = 0; i < 100; i++) {
        WSS_free((void **)&array[i]);
    }
    WSS_free((void **)&array);
}

Test(strinarray, test_string_in_array) {
    int i;
    int length = 100;
    char **array = (char **) WSS_calloc(length, sizeof(char *));

    for (i = 0; i < length; i++) {
        array[i] = WSS_malloc(3);
        sprintf(array[i], "%d", i);
    }

    cr_assert(strinarray("87", (const char **)array, length) == 0);
    cr_assert(strinarray("34", (const char **)array, length) == 0);
    cr_assert(strinarray("0", (const char **)array, length) == 0);
    cr_assert(strinarray("99", (const char **)array, length) == 0);
    cr_assert(strinarray("37", (const char **)array, length) == 0);
    cr_assert(strinarray("100", (const char **)array, length) == -1);
    cr_assert(strinarray("testing123", (const char **)array, length) == -1);
    cr_assert(strinarray("lol", (const char **)array, length) == -1);

    for (i = 0; i < length; i++) {
        WSS_free((void **)&array[i]);
    }
    WSS_free((void **)&array);
}

TestSuite(strload, .init = setup, .fini = teardown);

Test(strload, none_existing_file) {
    int n;
    char *content;

    n = strload("lollern.txt", &content); 

    cr_assert(n == 0);
    cr_assert(content == NULL);
}

Test(strload, load_txt_file) {
    int n;
    char *content;

    n = strload("resources/test.txt", &content); 

    cr_assert(n == 34);
    cr_assert(strncmp(content, "This is a file, used for testing.\n", n) == 0);

    WSS_free((void **)&content);
}
