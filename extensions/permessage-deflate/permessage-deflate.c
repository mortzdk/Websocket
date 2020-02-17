#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <regex.h>
#include <errno.h>
#include <zlib.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <limits.h>

#include "permessage-deflate.h"
#include "uthash.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define EXT_CHUNK_SIZE                 "chunk_size"
#define EXT_MEMORY_LEVEL               "memory_level"
#define EXT_SERVER_NO_CONTEXT_TAKEOVER "server_no_context_takeover"
#define EXT_CLIENT_NO_CONTEXT_TAKEOVER "client_no_context_takeover"
#define EXT_SERVER_MAX_WINDOW_BITS     "server_max_window_bits"
#define EXT_CLIENT_MAX_WINDOW_BITS     "client_max_window_bits"

#define MAX_CHUNK_SIZE UINT_MAX
#define MIN_CHUNK_SIZE 1
#define SERVER_MAX_MEM_LEVEL 9
#define SERVER_MIN_MEM_LEVEL 1
#define SERVER_MAX_WINDOW_BITS 15
#define SERVER_MIN_WINDOW_BITS 8
#define CLIENT_MAX_WINDOW_BITS 15
#define CLIENT_MIN_WINDOW_BITS 9 // Is really 8, but zlib does not support value 8


// Define a Websocket frame as exactly defined by the WSServer.
typedef struct {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;
    uint8_t opcode;
    bool mask;
    uint64_t payloadLength;
    char maskingKey[4];
    char *extensionData;
    uint64_t extensionDataLength;
    char *applicationData;
    uint64_t applicationDataLength;
} frame_t;

// Parameters for PMCE
typedef struct {
    bool server_no_context_takeover;
    bool client_no_context_takeover;
    uint8_t server_max_window_bits;
    uint8_t client_max_window_bits;
} param_t;


// Compressor structure for session 
typedef struct {
    int fd;
    z_stream compressor;
    z_stream decompressor;
    param_t params;
    UT_hash_handle hh;
} wss_comp_t;

/**
 * A lock that ensures the hash table is used atomically
 */
pthread_mutex_t lock;

/**
 * Global hashtable of subprotocols
 */
wss_comp_t *compressors = NULL;

/**
 * Default values
 */
static bool default_client_no_context_takeover = false;
static bool default_server_no_context_takeover = false;
static int default_chunk_size = 32768;
static int default_memory_level = 4;
static int default_server_window_bits = 15;
static int default_client_window_bits = 15;

/**
 * Trims a string for leading and trailing whitespace.
 *
 * @param   str     [char *]    "The string to trim"
 * @return          [char *]    "The input string without leading and trailing spaces"
 */
inline static char *trim(char *str) {
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;

    if ( (NULL == str) ) {
        return NULL;
    }

    if ( str[0] == '\0' ) {
        return str;
    }

    len = strlen(str);
    endp = str + len;

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while (isspace((unsigned char) *frontp)) {
        ++frontp;
    }

    if (endp != frontp) {
        while (isspace((unsigned char) *(--endp)) && endp != frontp) {}
    }

    if (str + len - 1 != endp) {
        *(endp + 1) = '\0';
    } else if (frontp != str &&  endp == frontp) {
        *str = '\0';
    }

    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.  Note the reuse
     * of endp to mean the front of the string buffer now.
     */
    endp = str;
    if (frontp != str) {
        while (*frontp) {
            *endp++ = *frontp++;
        }
        *endp = '\0';
    }

    return str;
}

static void parse_param(const char *param, param_t *p) {
    int bits, j;
    char *sep, *sepptr;
    size_t params_length = strlen(param);
    char buffer[params_length+1];

    memset(buffer, '\0', params_length+1);
    memcpy(buffer, param, params_length);

    // Server defaults
    p->client_no_context_takeover = default_client_no_context_takeover;
    p->server_no_context_takeover = default_server_no_context_takeover;
    p->server_max_window_bits = default_server_window_bits;
    p->client_max_window_bits = 0;

    sep = trim(strtok_r(buffer, ";", &sepptr));
    while (NULL != sep) {
        if ( strncmp(EXT_CLIENT_MAX_WINDOW_BITS, sep, strlen(EXT_CLIENT_MAX_WINDOW_BITS)) == 0) {
            j = strlen(EXT_CLIENT_MAX_WINDOW_BITS);
            while ( sep[j] != '=' && sep[j] != '\0' ) {
                j++;
            }

            if (sep[j] != '\0') {
                j++;
                bits = strtol(sep+j, NULL, 10);
                p->client_max_window_bits = MIN(bits, CLIENT_MAX_WINDOW_BITS);
            } else {
                // Default
                p->client_max_window_bits = default_client_window_bits;
            }
        } else if ( strncmp(EXT_SERVER_MAX_WINDOW_BITS, sep, strlen(EXT_SERVER_MAX_WINDOW_BITS)) == 0) {
            j = strlen(EXT_CLIENT_MAX_WINDOW_BITS);
            while ( sep[j] != '=' ) {
                j++;
            }

            j++;
            bits = strtol(sep+j, NULL, 10);
            p->server_max_window_bits = MIN(bits, SERVER_MAX_WINDOW_BITS);
        } else if ( strncmp(EXT_CLIENT_NO_CONTEXT_TAKEOVER, sep, strlen(EXT_CLIENT_NO_CONTEXT_TAKEOVER)) == 0) {
            p->client_no_context_takeover = true;
        } else if ( strncmp(EXT_SERVER_NO_CONTEXT_TAKEOVER, sep, strlen(EXT_SERVER_NO_CONTEXT_TAKEOVER)) == 0) {
            p->server_no_context_takeover = true;
        }

        sep = trim(strtok_r(NULL, ";", &sepptr));
    }
}

static char * negotiate(char *param, wss_comp_t *comp) {
    size_t snct_length = 0, cnct_length = 0, cmwb_length = 0, smwb_length = 0, accepted_length = 0, written = 0;
    char *accepted = NULL;
    param_t *p = &comp->params;

    parse_param(param, p);

    if (p->server_no_context_takeover) {
        // server_no_contect_takeover
        snct_length = strlen(EXT_SERVER_NO_CONTEXT_TAKEOVER)+1;
        accepted_length += snct_length;
    }

    if (p->client_no_context_takeover) {
        // client_no_context_takeover
        cnct_length = strlen(EXT_CLIENT_NO_CONTEXT_TAKEOVER)+1;
        accepted_length += cnct_length;
    }

    if (p->client_max_window_bits > 0) {
        // client_max_window_bits=%d;
        cmwb_length = strlen(EXT_CLIENT_MAX_WINDOW_BITS) + 1 + (log10(p->client_max_window_bits)+1) + 1;
        accepted_length += cmwb_length;
    }

    // server_max_window_bits=%d
    smwb_length = strlen(EXT_SERVER_MAX_WINDOW_BITS) + 1 + (log10(p->server_max_window_bits)+1);
    accepted_length += smwb_length;

    if (NULL != (accepted = malloc((accepted_length+1)*sizeof(char)))) {
        memset(accepted, '\0', accepted_length+1);

        if (p->server_no_context_takeover) {
            // server_no_contect_takeover;
            snprintf(accepted+written, snct_length+1, "%s;", EXT_SERVER_NO_CONTEXT_TAKEOVER);
            written += snct_length;
        }

        if (p->client_no_context_takeover) {
            // client_no_context_takeover;
            snprintf(accepted+written, cnct_length+1, "%s;", EXT_CLIENT_NO_CONTEXT_TAKEOVER);
            written += cnct_length;
        }

        if (p->client_max_window_bits > 0) {
            // client_max_window_bits=%d;
            snprintf(accepted+written, cmwb_length+1, "%s=%d;", EXT_CLIENT_MAX_WINDOW_BITS, p->client_max_window_bits);
            written += cmwb_length;
        }

        // server_max_window_bits=%d
        snprintf(accepted+written, smwb_length+1, "%s=%d", EXT_SERVER_MAX_WINDOW_BITS, p->server_max_window_bits);
    }

    return accepted;    
}

static bool init_comp(wss_comp_t *comp) {
    /* allocate inflate state */
    comp->decompressor.zalloc   = Z_NULL;
    comp->decompressor.zfree    = Z_NULL;
    comp->decompressor.opaque   = Z_NULL;
    comp->decompressor.avail_in = 0;
    comp->decompressor.next_in  = Z_NULL;

    if (Z_OK != inflateInit2(&comp->decompressor, -comp->params.client_max_window_bits)) {
        free(comp);
        return false;
    }

    /* allocate inflate state */
    comp->compressor.zalloc = Z_NULL;
    comp->compressor.zfree  = Z_NULL;
    comp->compressor.opaque = Z_NULL;

    if (Z_OK != deflateInit2(&comp->compressor, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -comp->params.server_max_window_bits, default_memory_level, Z_DEFAULT_STRATEGY)) {

        inflateEnd(&comp->decompressor);
        free(comp);
        return false;
    }

    return true;
}

/**
 * Event called when extension is initialized.
 *
 * @param 	config	[char *]     "The configuration of the extension"
 * @return 	        [void]
 */
void onInit(char *config) {
    int err;
    long int val;
    size_t i, j;
    regex_t re;
    size_t nmatch = 8;
    regmatch_t matches[nmatch];
    size_t params_length = strlen(config);
    char buffer[params_length+1];
    const char *reg_str = "^(\\s*((server_no_context_takeover)|(server_max_window_bits\\s*=\\s*[0-9]+)|(client_max_window_bits\\s*=\\s*[0-9]+)|(memory_level\\s*=\\s*[0-9]+)|(chunk_size\\s*=\\s*[0-9]+))\\s*;?\\s*)*$";

    if ( NULL == config ) {
        return;
    }

    if ( (err = regcomp(&re, reg_str, REG_EXTENDED)) != 0) {
        return;
    } 

    memset(buffer, '\0', params_length+1);
    memcpy(buffer, config, params_length);

    err = regexec(&re, buffer, nmatch, matches, 0);
    if ( err != 0 ) {
        return;
    }

    for (i = 3; i < nmatch; i++) {
        if (matches[i].rm_so == -1) {
            continue;
        }

        buffer[matches[i].rm_eo] = '\0';

        if ( strncmp(EXT_CHUNK_SIZE, buffer+matches[i].rm_so, strlen(EXT_CHUNK_SIZE)) == 0) {
            j = matches[i].rm_so;
            while (buffer[j] != '=') {
                j++;
            }

            j++;
            val = strtol(buffer+j, NULL, 10);
            if (val < MIN_CHUNK_SIZE || MAX_CHUNK_SIZE < val) {
                continue;
            }
            default_chunk_size = val;
        } else 
        if ( strncmp(EXT_MEMORY_LEVEL, buffer+matches[i].rm_so, strlen(EXT_MEMORY_LEVEL)) == 0) {
            j = matches[i].rm_so;
            while (buffer[j] != '=') {
                j++;
            }

            j++;
            val = strtol(buffer+j, NULL, 10);
            if (val < SERVER_MIN_MEM_LEVEL || SERVER_MAX_MEM_LEVEL < val) {
                continue;
            }
            default_memory_level = val;
        } else if ( strncmp(EXT_SERVER_MAX_WINDOW_BITS, buffer+matches[i].rm_so, strlen(EXT_SERVER_MAX_WINDOW_BITS)) == 0) {
            j = matches[i].rm_so;
            while (buffer[j] != '=') {
                j++;
            }

            j++;
            val = strtol(buffer+j, NULL, 10);
            if (val < SERVER_MIN_WINDOW_BITS || SERVER_MAX_WINDOW_BITS < val) {
                continue;
            }

            default_server_window_bits = val;
        } else if ( strncmp(EXT_CLIENT_MAX_WINDOW_BITS, buffer+matches[i].rm_so, strlen(EXT_CLIENT_MAX_WINDOW_BITS)) == 0) {
            j = matches[i].rm_so;
            while (buffer[j] != '=') {
                j++;
            }

            j++;
            val = strtol(buffer+j, NULL, 10);
            if (val < CLIENT_MIN_WINDOW_BITS || CLIENT_MAX_WINDOW_BITS < val) {
                continue;
            }
            default_client_window_bits = val;
        } else if ( strncmp(EXT_SERVER_NO_CONTEXT_TAKEOVER, buffer+matches[i].rm_so, strlen(EXT_SERVER_NO_CONTEXT_TAKEOVER)) == 0) {
            default_server_no_context_takeover = true;
        }
    }

    regfree(&re);

    pthread_mutex_init(&lock, NULL);
}

void onOpen(int fd, char *param, char **accepted, bool *valid) {
    int err, j;
    long int val;
    size_t i;
    regex_t re;
    size_t nmatch = 7;
    regmatch_t matches[nmatch];
    size_t params_length = strlen(param);
    wss_comp_t *comp;
    char buffer[params_length+1];
    const char *reg_str = "^(\\s*((server_no_context_takeover)|(client_no_context_takeover)|(server_max_window_bits\\s*=\\s*[0-9]+)|(client_max_window_bits(\\s*=\\s*[0-9]+)?))*\\s*;?\\s*)*$";

    if ( NULL == param ) {
        if ( pthread_mutex_lock(&lock) != 0 ) {
            *valid = false;
            return;
        }

        if ( NULL == (comp = malloc(sizeof(wss_comp_t))) ) {
            *valid = false;
            return;
        }

        comp->fd = fd;
        comp->params.client_max_window_bits = default_client_window_bits;
        comp->params.server_max_window_bits = default_server_window_bits;
        comp->params.client_no_context_takeover = default_client_no_context_takeover;
        comp->params.server_no_context_takeover = default_server_no_context_takeover;

        if (! init_comp(comp)) {
            free(comp);
            *valid = false;
            return;
        }

        HASH_ADD_INT(compressors, fd, comp);

        pthread_mutex_unlock(&lock);

        *valid = true;
        return;
    }

    if ( (err = regcomp(&re, reg_str, REG_EXTENDED)) != 0) {
        *valid = false;
        return;
    } 

    memset(buffer, '\0', params_length+1);
    memcpy(buffer, param, params_length);

    err = regexec(&re, buffer, nmatch, matches, 0);
    if ( err != 0 ) {
        *valid = false;
        return;
    }

    for (i = 3; i < nmatch-1; i++) {
        if (matches[i].rm_so == -1) {
            continue;
        }

        buffer[matches[i].rm_eo] = '\0';

        if ( strncmp(EXT_CLIENT_MAX_WINDOW_BITS, buffer+matches[i].rm_so, strlen(EXT_CLIENT_MAX_WINDOW_BITS)) == 0) {
            j = matches[i].rm_so+strlen(EXT_CLIENT_MAX_WINDOW_BITS);
            while (buffer[j] != '=' && buffer[j] != '\0') {
                j++;
            }

            if (buffer[j] != '\0') {
                j++;
                val = strtol(buffer+j, NULL, 10);
                if (CLIENT_MIN_WINDOW_BITS < val || CLIENT_MAX_WINDOW_BITS > val) {
                    *valid = false;
                    return;
                }
            }
        } else if ( strncmp(EXT_SERVER_MAX_WINDOW_BITS, buffer+matches[i].rm_so, strlen(EXT_SERVER_MAX_WINDOW_BITS)) == 0) {
            j = matches[i].rm_so+strlen(EXT_SERVER_MAX_WINDOW_BITS);
            while (buffer[j] != '=') {
                j++;
            }

            j++;
            val = strtol(buffer+j, NULL, 10);
            if (SERVER_MIN_WINDOW_BITS < val || SERVER_MAX_WINDOW_BITS > val) {
                *valid = false;
                return;
            }
        }
    }

    regfree(&re);

    if ( pthread_mutex_lock(&lock) != 0 ) {
        *valid = false;
        return;
    }

    if ( NULL == (comp = malloc(sizeof(wss_comp_t))) ) {
        *valid = false;
        return;
    }

    comp->fd = fd;
    comp->params.client_max_window_bits = default_client_window_bits;
    comp->params.server_max_window_bits = default_server_window_bits;
    comp->params.client_no_context_takeover = default_client_no_context_takeover;
    comp->params.server_no_context_takeover = default_server_no_context_takeover;

    *accepted = negotiate(param, comp);

    if (! init_comp(comp)) {
        free(comp);
        *valid = false;
        return;
    }

    HASH_ADD_INT(compressors, fd, comp);

    pthread_mutex_unlock(&lock);

    *valid = true;

}

void inFrame(int fd, void *f) {
}

void inFrames(int fd, void **fs, size_t len) {
    frame_t **frames = (frame_t **)fs;
    size_t j, size;
    char *message = NULL;
    char *payload = NULL;
    wss_comp_t *comp = NULL;
    size_t payload_length = 0;
    size_t current_length = 0;
    size_t message_length = 0;
    int flush_mask = Z_SYNC_FLUSH;

    if (! frames[0]->rsv1) {
        return;
    }

    if ( pthread_mutex_lock(&lock) != 0 ) {
        return;
    }

    HASH_FIND_INT(compressors, &fd, comp);

    pthread_mutex_unlock(&lock);

    if (NULL == comp) {
        return;
    }

    for (j = 0; j < len; j++) {
        payload_length += frames[j]->payloadLength; 
    }
    payload_length += 4;

    if (NULL == (payload = malloc(payload_length+1))) {
        return;
    }
    memset(payload, '\0', payload_length+1);

    for (j = 0; j < len; j++) {
        memcpy(payload+current_length, frames[j]->extensionData, frames[j]->extensionDataLength);
        current_length += frames[j]->extensionDataLength;

        memcpy(payload+current_length, frames[j]->applicationData, frames[j]->applicationDataLength);
        current_length += frames[j]->applicationDataLength;
    }

    memcpy(payload+current_length, "\x00\x00\xff\xff", 4);

    comp->decompressor.avail_in = payload_length;
    comp->decompressor.next_in = (unsigned char *)payload;

    if (comp->params.client_no_context_takeover) {
        flush_mask = Z_FULL_FLUSH;
    }

    // Decompress data
    do {
        if (NULL == (message = realloc(message, (message_length+default_chunk_size+1)*sizeof(char)))) {
            free(payload);
            free(message);
           return; 
        }
        memset(message+message_length, '\0', default_chunk_size+1); 

        comp->decompressor.avail_out = default_chunk_size;
        comp->decompressor.next_out = (unsigned char *)message+message_length;
        switch (inflate(&comp->decompressor, flush_mask)) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                free(payload);
                free(message);
                return;
        }
        message_length += default_chunk_size - comp->decompressor.avail_out;
    } while (comp->decompressor.avail_out == 0);

    // unset rsv1 bit
    frames[0]->rsv1 = false;
    current_length = 0;

    // Reallocate application data to contain the decompressed data in the same
    // amount of frames
    for (j = 0; j < len; j++) {
        if ( NULL != frames[j]->extensionData ) {
            free(frames[j]->extensionData);
        }
        frames[j]->extensionDataLength = 0;

        if (j+1 != len) {
            size = message_length/len;
        } else {
            size = message_length-current_length;
        }

        if ( NULL == (frames[j]->applicationData = realloc(frames[j]->applicationData, size))) {
            free(payload);
            free(message);
            return;
        }
        memcpy(frames[j]->applicationData, message+current_length, size);
        current_length += size;
        frames[j]->applicationDataLength = size;
        frames[j]->payloadLength = size;
    }

    free(payload);
    free(message);
}

void outFrame(int fd, void *f) {
}

void outFrames(int fd, void **fs, size_t len) {
    frame_t **frames = (frame_t **)fs;
    size_t j, size;
    char *message = NULL;
    char *payload = NULL;
    size_t payload_length = 0;
    size_t current_length = 0;
    size_t message_length = 0;
    int flush_mask = Z_SYNC_FLUSH;
    wss_comp_t *comp;

    if (frames[0]->opcode >= 0x8 && frames[0]->opcode <= 0xA) {
        return;
    }

    if ( pthread_mutex_lock(&lock) != 0 ) {
        return;
    }

    HASH_FIND_INT(compressors, &fd, comp);

    pthread_mutex_unlock(&lock);

    if ( NULL == comp ) {
        return;
    }

    for (j = 0; j < len; j++) {
        payload_length += frames[j]->payloadLength; 
    }

    if (NULL == (payload = malloc(payload_length+1))) {
        return;
    }
    memset(payload, '\0', payload_length+1);

    for (j = 0; j < len; j++) {
        memcpy(payload+current_length, frames[j]->extensionData, frames[j]->extensionDataLength);
        current_length += frames[j]->extensionDataLength;

        memcpy(payload+current_length, frames[j]->applicationData, frames[j]->applicationDataLength);
        current_length += frames[j]->applicationDataLength;
    }

    comp->compressor.avail_in = payload_length;
    comp->compressor.next_in = (unsigned char *)payload;

    if (comp->params.server_no_context_takeover) {
        flush_mask = Z_FULL_FLUSH;
    }

    // Compress whole message
    do {
        if (NULL == (message = realloc(message, (message_length+default_chunk_size+1)*sizeof(char)))) {
            free(payload);
            free(message);
           return; 
        }
        memset(message+message_length, '\0', default_chunk_size+1); 

        comp->compressor.avail_out = default_chunk_size;
        comp->compressor.next_out = (unsigned char *)message+message_length;
        switch (deflate(&comp->compressor, flush_mask)) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                free(payload);
                free(message);
                return;
        }
        message_length += default_chunk_size - comp->compressor.avail_out;
    } while (comp->compressor.avail_out == 0);

    if (message_length >= 5 && memcmp(message+message_length-4, "\x00\x00\xff\xff", 4) == 0) {
        memset(message+message_length-4, '\0', 4);
        message_length -= 4;
    }

    // set rsv1 bit
    frames[0]->rsv1 = true;

    current_length = 0;

    // Reallocate application data to contain the compressed data in the same
    // amount of frames
    for (j = 0; j < len; j++) {
        if ( NULL != frames[j]->extensionData ) {
            free(frames[j]->extensionData);
        }
        frames[j]->extensionDataLength = 0;

        if ( j+1 != len ) {
            size = message_length-current_length;
        } else {
            size = message_length/len;
        }

        if ( NULL == (frames[j]->applicationData = realloc(frames[j]->applicationData, size)) ) {
            free(payload);
            free(message);
            return;
        }
        memcpy(frames[j]->applicationData, message+current_length, size);
        current_length += size;
        frames[j]->applicationDataLength = size;
        frames[j]->payloadLength = size;
    }

    free(payload);
    free(message);
}


/**
 * Event called when the session is closed.
 *
 * @param 	fd	[int]        "The filedescriptor of the session"
 *
 * @return 	    [void]
 */
void onClose(int fd) {
    wss_comp_t *comp;
    if ( pthread_mutex_lock(&lock) != 0) {
        return;
    }

    HASH_FIND_INT(compressors, &fd, comp);

    if ( NULL != comp ) {
        HASH_DEL(compressors, comp);

        (void)inflateEnd(&comp->decompressor);
        (void)deflateEnd(&comp->compressor);

        free(comp);
    }

    pthread_mutex_unlock(&lock);
}

/**
 * Event called when the extension should be destroyed.
 *
 * @param 	fd	[int]        "The filedescriptor of the session"
 *
 * @return 	    [void]
 */
void onDestroy() {
    wss_comp_t *comp, *tmp;

    if ( pthread_mutex_lock(&lock) != 0) {
        return;
    }

    HASH_ITER(hh, compressors, comp, tmp) {
        if ( NULL != comp ) {
            HASH_DEL(compressors, comp);

            (void)inflateEnd(&comp->decompressor);
            (void)deflateEnd(&comp->compressor);

            free(comp);
        }
    }

    pthread_mutex_unlock(&lock);
    pthread_mutex_destroy(&lock);
}
