#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <criterion/criterion.h>

#include "str.h"
#include "alloc.h"

#if defined(USE_OPENSSL) | defined(USE_BORINGSSL) | defined(USE_LIBRESSL)

#include <openssl/sha.h>

#elif defined(USE_WOLFSSL)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include <wolfssl/wolfcrypt/sha.h>
#pragma GCC diagnostic pop

#define SHA_DIGEST_LENGTH 20

#else

#include "sha1.h"
#define SHA_DIGEST_LENGTH 20

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
    char output[1];
    char *out = &output[0];
    char *string = "";
    size_t len = bin2hex((const char unsigned *) string, strlen(string), &out);
    cr_assert(len == 0);
    cr_assert(output[0] == '\0'); 
}

Test(bin2hex, simple_string) {
    size_t len;
    char output[11];
    char *out = &output[0];
    const char unsigned *string = (const char unsigned *)WSS_malloc(6);
    sprintf((char *)string, "%s", "12345");

    len = bin2hex(string, strlen((char *)string), &out);

    cr_assert(len == 10); 
    cr_assert(strncmp(output, "3132333435", len) == 0); 
    cr_assert(output[10] == '\0'); 

    WSS_free((void **) &string);
}

Test(bin2hex, sha1_string) {
    size_t len;
    char output[2*SHA_DIGEST_LENGTH+1];
    char *out = &output[0];
    char sha1Key[SHA_DIGEST_LENGTH];
    const char unsigned *string = (const char unsigned *)WSS_malloc(6);
    sprintf((char *)string, "%s", "12345");

#if defined(USE_OPENSSL)
    SHA1((const unsigned char *)string, strlen((char *)string), (unsigned char*) sha1Key);
#elif defined(USE_WOLFSSL)
    Sha sha;
    wc_InitSha(&sha);
    wc_ShaUpdate(&sha, (const unsigned char *) string, strlen((char *)string));
    wc_ShaFinal(&sha, (unsigned char *) sha1Key);
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
    len = bin2hex((const unsigned char *)sha1Key, SHA_DIGEST_LENGTH, &out);

    cr_assert(len == 2*SHA_DIGEST_LENGTH); 
    cr_assert(strncmp(output, "8cb2237d0679ca88db6464eac60da96345513964", 2*SHA_DIGEST_LENGTH));
    cr_assert(output[2*SHA_DIGEST_LENGTH] == '\0'); 

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
