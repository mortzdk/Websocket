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

#define EXT_CHUNK_SIZE                 "chunk_size"
#define EXT_MEMORY_LEVEL               "memory_level"
#define EXT_COMPRESSION_LEVEL          "compression_level"
#define EXT_SERVER_NO_CONTEXT_TAKEOVER "server_no_context_takeover"
#define EXT_CLIENT_NO_CONTEXT_TAKEOVER "client_no_context_takeover"
#define EXT_SERVER_MAX_WINDOW_BITS     "server_max_window_bits"
#define EXT_CLIENT_MAX_WINDOW_BITS     "client_max_window_bits"

#define MAX_CHUNK_SIZE UINT_MAX
#define MIN_CHUNK_SIZE 1
#define SERVER_MIN_COMPRESSION_LEVEL Z_NO_COMPRESSION 
#define SERVER_MAX_COMPRESSION_LEVEL Z_BEST_COMPRESSION 
#define SERVER_MAX_MEM_LEVEL MAX_MEM_LEVEL
#define SERVER_MIN_MEM_LEVEL 1
#define SERVER_MAX_WINDOW_BITS 15
#define SERVER_MIN_WINDOW_BITS 8
#define CLIENT_MAX_WINDOW_BITS 15
#define CLIENT_MIN_WINDOW_BITS 8 

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

// Structure containing allocators
typedef struct {
    void *(*malloc)(size_t);
    void *(*realloc)(void *, size_t);
    void (*free)(void *);
} allocators;

/**
 * Global allocators
 */
allocators allocs = {
    malloc,
    realloc,
    free
};

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
static int default_chunk_size                  = 32768;
static int default_memory_level                = 8;
static int default_compression_level           = Z_DEFAULT_COMPRESSION;
static int default_server_window_bits          = 15;
static int default_client_window_bits          = 15;

/**
 * Trims a string for leading and trailing whitespace.
 *
 * @param   str     [char *]    "The string to trim"
 * @return          [char *]    "The input string without leading and trailing spaces"
 */
static inline char *trim(char* str)
{
    if ( unlikely(NULL == str) ) {
        return NULL;
    }

    if ( unlikely(str[0] == '\0') ) {
        return str;
    }

    int start, end = strlen(str);
    for (start = 0; likely(isspace(str[start])); ++start) {}
    if (likely(str[start])) {
        while (end > 0 && isspace(str[end-1]))
            --end;
        memmove(str, &str[start], end - start);
    }
    str[end - start] = '\0';

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
    while ( likely(NULL != sep) ) {
        if ( strncmp(EXT_CLIENT_MAX_WINDOW_BITS, sep, strlen(EXT_CLIENT_MAX_WINDOW_BITS)) == 0) {
            j = strlen(EXT_CLIENT_MAX_WINDOW_BITS);
            while ( sep[j] != '=' && sep[j] != '\0' ) {
                j++;
            }

            if (sep[j] != '\0') {
                j++;
                bits = strtol(sep+j, NULL, 10);

                p->client_max_window_bits = WSS_MIN(bits, CLIENT_MAX_WINDOW_BITS);
            } else {
                // Default
                p->client_max_window_bits = default_client_window_bits;
            }
        } else if ( strncmp(EXT_SERVER_MAX_WINDOW_BITS, sep, strlen(EXT_SERVER_MAX_WINDOW_BITS)) == 0) {
            j = strlen(EXT_SERVER_MAX_WINDOW_BITS);
            while ( sep[j] != '=' ) {
                j++;
            }

            j++;
            bits = strtol(sep+j, NULL, 10);

            // Is really 8, but zlib does not support value 8 so increase to 9
            if (bits == SERVER_MIN_WINDOW_BITS) {
                bits++;
            }

            p->server_max_window_bits = WSS_MIN(bits, SERVER_MAX_WINDOW_BITS);
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
        // server_no_context_takeover
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

    if ( likely(NULL != (accepted = allocs.malloc((accepted_length+1)*sizeof(char)))) ) {
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

static void *zalloc(void *opaque, unsigned items, unsigned size) {
    WSS_UNUSED(opaque);
    return allocs.malloc(size*items);
}

static void zfree(void *opaque, void *address) {
    WSS_UNUSED(opaque);
    allocs.free(address);
}

static bool init_comp(wss_comp_t *comp) {
    /* allocate inflate state */
    comp->decompressor.zalloc   = zalloc;
    comp->decompressor.zfree    = zfree;
    comp->decompressor.opaque   = Z_NULL;
    comp->decompressor.avail_in = 0;
    comp->decompressor.next_in  = Z_NULL;

    if (comp->params.client_max_window_bits > 0) {
        // If client_max_window_bits was negotiated
        if ( unlikely(Z_OK != inflateInit2(&comp->decompressor, -comp->params.client_max_window_bits)) ) {
            return false;
        }
    } else {
        // If client_max_window_bits was not negotiated, the default is -15 = 32768 byte sliding window
        if ( unlikely(Z_OK != inflateInit2(&comp->decompressor, -CLIENT_MAX_WINDOW_BITS)) ) {
            return false;
        }
    }

    /* allocate inflate state */
    comp->compressor.zalloc = zalloc;
    comp->compressor.zfree  = zfree;
    comp->compressor.opaque = Z_NULL;

    if ( unlikely(Z_OK != deflateInit2(&comp->compressor, default_compression_level, Z_DEFLATED, -comp->params.server_max_window_bits, default_memory_level, Z_DEFAULT_STRATEGY)) ) {

        inflateEnd(&comp->decompressor);
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
    const char *reg_str = "^(\\s*((server_no_context_takeover)|(server_max_window_bits\\s*=\\s*[0-9]+)|(client_max_window_bits\\s*=\\s*[0-9]+)|(memory_level\\s*=\\s*[0-9]+)|(compression_level\\s*=\\s*[0-9]+)|(chunk_size\\s*=\\s*[0-9]+))\\s*;?\\s*)*$";

    if ( NULL == config ) {
        return;
    }
    size_t params_length = strlen(config);
    char buffer[params_length+1];

    if ( unlikely((err = regcomp(&re, reg_str, REG_EXTENDED)) != 0) ) {
        return;
    } 

    memset(buffer, '\0', params_length+1);
    memcpy(buffer, config, params_length);

    err = regexec(&re, buffer, nmatch, matches, 0);
    if ( unlikely(err != 0) ) {
        return;
    }

    for (i = 3; likely(i < nmatch); i++) {
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
        } else if ( strncmp(EXT_COMPRESSION_LEVEL, buffer+matches[i].rm_so, strlen(EXT_COMPRESSION_LEVEL)) == 0) {
            j = matches[i].rm_so;
            while (buffer[j] != '=') {
                j++;
            }

            j++;
            val = strtol(buffer+j, NULL, 10);
            if (val < SERVER_MIN_COMPRESSION_LEVEL || SERVER_MAX_COMPRESSION_LEVEL < val) {
                continue;
            }
            default_compression_level = val;
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

            // Is really 8, but zlib does not support value 8 so increase to 9
            if (val == SERVER_MIN_WINDOW_BITS) {
                val++;
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

/**
 * Sets the allocators to use instead of the default ones
 *
 * @param 	extmalloc	[WSS_malloc_t]     "The malloc function"
 * @param 	extrealloc	[WSS_realloc_t]    "The realloc function"
 * @param 	extfree	    [WSS_free_t]       "The free function"
 * @return 	            [void]
 */
void setAllocators(WSS_malloc_t extmalloc, WSS_realloc_t extrealloc, WSS_free_t extfree) {
    allocs.malloc = extmalloc;
    allocs.realloc = extrealloc;
    allocs.free = extfree;
}

/**
 * Event called when parameters are available for the pcme i.e. when the
 * connection is opened.
 *
 * @param 	fd	        [int]        "The filedescriptor of the session"
 * @param 	param	    [char *]     "The parameters to the PCME"
 * @param 	accepted	[char *]     "The accepted parameters to the PCME"
 * @param 	valid	    [bool *]     "A pointer to a boolean, that should state whether the parameters are accepted"
 * @return 	            [void]
 */
void onOpen(int fd, char *param, char **accepted, bool *valid) {
    int err, j;
    long int val;
    size_t i;
    regex_t re;
    size_t nmatch = 7;
    regmatch_t matches[nmatch];
    wss_comp_t *comp;
    const char *reg_str = "^(\\s*((server_no_context_takeover)|(client_no_context_takeover)|(server_max_window_bits\\s*=\\s*[0-9]+)|(client_max_window_bits(\\s*=\\s*[0-9]+)?))*\\s*;?\\s*)*$";

    if ( NULL == param ) {
        if ( unlikely(pthread_mutex_lock(&lock) != 0) ) {
            *valid = false;
            return;
        }

        if ( unlikely(NULL == (comp = allocs.malloc(sizeof(wss_comp_t)))) ) {
            *valid = false;
            return;
        }

        comp->fd = fd;
        comp->params.client_max_window_bits = default_client_window_bits;
        comp->params.server_max_window_bits = default_server_window_bits;
        comp->params.client_no_context_takeover = default_client_no_context_takeover;
        comp->params.server_no_context_takeover = default_server_no_context_takeover;

        if ( unlikely(! init_comp(comp)) ) {
            allocs.free(comp);
            *valid = false;
            return;
        }

        HASH_ADD_INT(compressors, fd, comp);

        pthread_mutex_unlock(&lock);

        *valid = true;
        return;
    }
    size_t params_length = strlen(param);
    char buffer[params_length+1];

    if ( unlikely((err = regcomp(&re, reg_str, REG_EXTENDED)) != 0) ) {
        *valid = false;
        return;
    } 

    memset(buffer, '\0', params_length+1);
    memcpy(buffer, param, params_length);

    err = regexec(&re, buffer, nmatch, matches, 0);
    if ( unlikely(err != 0) ) {
        *valid = false;
        return;
    }

    for (i = 3; likely(i < nmatch-1); i++) {
        if (matches[i].rm_so == -1) {
            continue;
        }

        buffer[matches[i].rm_eo] = '\0';

        if ( strncmp(EXT_CLIENT_MAX_WINDOW_BITS, buffer+matches[i].rm_so, strlen(EXT_CLIENT_MAX_WINDOW_BITS)) == 0 ) {
            j = matches[i].rm_so+strlen(EXT_CLIENT_MAX_WINDOW_BITS);
            while (buffer[j] != '=' && buffer[j] != '\0') {
                j++;
            }

            if (buffer[j] != '\0') {
                j++;
                val = strtol(buffer+j, NULL, 10);
                if ( val < CLIENT_MIN_WINDOW_BITS || CLIENT_MAX_WINDOW_BITS < val) {
                    *valid = false;
                    return;
                }
            }
        } else if ( strncmp(EXT_SERVER_MAX_WINDOW_BITS, buffer+matches[i].rm_so, strlen(EXT_SERVER_MAX_WINDOW_BITS)) == 0 ) {
            j = matches[i].rm_so+strlen(EXT_SERVER_MAX_WINDOW_BITS);
            while (buffer[j] != '=') {
                j++;
            }

            j++;
            val = strtol(buffer+j, NULL, 10);
            if ( val < SERVER_MIN_WINDOW_BITS || SERVER_MAX_WINDOW_BITS < val) {
                *valid = false;
                return;
            }
        }
    }

    regfree(&re);

    if ( unlikely(pthread_mutex_lock(&lock) != 0) ) {
        *valid = false;
        return;
    }

    if ( unlikely(NULL == (comp = allocs.malloc(sizeof(wss_comp_t)))) ) {
        *valid = false;
        return;
    }

    comp->fd = fd;
    comp->params.client_max_window_bits = default_client_window_bits;
    comp->params.server_max_window_bits = default_server_window_bits;
    comp->params.client_no_context_takeover = default_client_no_context_takeover;
    comp->params.server_no_context_takeover = default_server_no_context_takeover;

    *accepted = negotiate(param, comp);

    if ( unlikely(! init_comp(comp)) ) {
        allocs.free(comp);
        *valid = false;
        return;
    }

    HASH_ADD_INT(compressors, fd, comp);

    *valid = true;

    pthread_mutex_unlock(&lock);
}

static inline bool writeFrames(wss_frame_t **frames, z_stream *comp, char *buffer, size_t *last_frame_length, size_t *next_start_index, size_t total_in, size_t payload_length, size_t last_index) {
    size_t j;
    char *tmp_payload;
    size_t written    = 0;
    size_t length     = default_chunk_size - comp->avail_out;
    size_t last_frame = floor(((double)(comp->total_in-total_in) / payload_length) * last_index);
    size_t size       = (size_t)ceil((double)length/(last_frame-*next_start_index+1));
    for (j = *next_start_index; likely(j <= last_frame); j++) {
        // Update last frame with lengths and reset last frame length since we've advanced to a new frame
        if ( likely(j != *next_start_index) ) {
            frames[j-1]->extensionDataLength = 0;
            frames[j-1]->applicationDataLength = *last_frame_length;
            frames[j-1]->payloadLength = frames[j-1]->applicationDataLength + frames[j-1]->extensionDataLength;

            *last_frame_length = 0;
        }

        // Adjust size for the last frame 
        if ( unlikely(written + size > length) ) {
            size = length - written;
        }

        if ( *last_frame_length+size > frames[j]->payloadLength ) {
            if ( unlikely(NULL == (tmp_payload = allocs.realloc(frames[j]->payload, *last_frame_length+size+1))) ) {
                return false;
            }
            frames[j]->payload = tmp_payload;
            frames[j]->payloadLength = *last_frame_length+size;
        }

        memcpy(frames[j]->payload+*last_frame_length, buffer+written, size);

        *last_frame_length += size;
        written            += size;
    }

    *next_start_index = j-1;

    return true;
}

/**
 * Event called when a full set of frames are received.
 *
 * @param 	fd	            [int]             "The filedescriptor of the session"
 * @param 	frames	        [wss_frame_t **]  "The websocket frames received"
 * @param 	frames_count	[size_t]          "The amount of frames"
 * @return 	                [void]
 */
void inFrames(int fd, wss_frame_t **frames, size_t frames_count) {
    size_t j, total_in;
    size_t last_frame_length    = 0;
    size_t next_start_index     = 0;
    size_t last_index           = frames_count-1;
    size_t payload_length       = 0;
    wss_comp_t *comp            = NULL;
    size_t current_length       = 0;
    int flush_mask = Z_SYNC_FLUSH;
    char buffer[default_chunk_size];

    if (! frames[0]->rsv1) {
        return;
    }

    if ( unlikely(pthread_mutex_lock(&lock) != 0) ) {
        return;
    }

    HASH_FIND_INT(compressors, &fd, comp);

    pthread_mutex_unlock(&lock);

    if ( unlikely(NULL == comp) ) {
        return;
    }

    for (j = 0; likely(j < frames_count); j++) {
        payload_length += frames[j]->payloadLength; 
    }
    payload_length += 4;

    char payload[payload_length+1];
    payload[payload_length] = '\0';

    for (j = 0; likely(j < frames_count); j++) {
        memcpy(payload+current_length, frames[j]->payload, frames[j]->payloadLength);
        current_length += frames[j]->payloadLength;
    }

    memcpy(payload+current_length, "\x00\x00\xff\xff", 4);

    comp->decompressor.avail_in = payload_length;
    comp->decompressor.next_in = (unsigned char *)payload;

    if (comp->params.client_no_context_takeover) {
        flush_mask = Z_FULL_FLUSH;
    }

    // Decompress data and put it back into frames
    total_in = comp->decompressor.total_in;
    do {
        comp->decompressor.avail_out = default_chunk_size;
        comp->decompressor.next_out = (unsigned char *)buffer;
        switch (inflate(&comp->decompressor, flush_mask)) {
            case Z_OK:
            case Z_STREAM_END:
            case Z_BUF_ERROR:
                break;
            default:
                return;
        }

        if ( unlikely(! writeFrames(frames, &comp->decompressor, buffer, &last_frame_length, &next_start_index, total_in, payload_length, last_index)) ) {
            return;
        }
    } while ( comp->decompressor.avail_out == 0 );

    // Set last frame length
    frames[last_index]->extensionDataLength = 0;
    frames[last_index]->applicationDataLength = last_frame_length;
    frames[last_index]->payloadLength = frames[last_index]->applicationDataLength + frames[last_index]->extensionDataLength;

    // Unset rsv1 bit of first frame
    frames[0]->rsv1 = 0;

    inflateReset(&comp->compressor);
}

/**
 * Event called when a full set of frames are about to be sent.
 *
 * @param 	fd	            [int]             "The filedescriptor of the session"
 * @param 	frames	        [wss_frame_t **]  "The websocket frames received"
 * @param 	frames_count	[size_t]          "The amount of frames"
 * @return 	                [void]
 */
/*
void outFrames(int fd, wss_frame_t **frames, size_t frame_count) {
    size_t j, size;
    char *message = NULL;
    size_t payload_length = 0;
    size_t current_length = 0;
    size_t message_length = 0;
    int flush_mask = Z_SYNC_FLUSH;
    wss_comp_t *comp;

    if ( unlikely(frames[0]->opcode >= 0x8 && frames[0]->opcode <= 0xA) ) {
        return;
    }

    if ( unlikely(pthread_mutex_lock(&lock) != 0) ) {
        return;
    }

    HASH_FIND_INT(compressors, &fd, comp);

    pthread_mutex_unlock(&lock);

    if ( unlikely(NULL == comp) ) {
        return;
    }

    for (j = 0; likely(j < frame_count); j++) {
        payload_length += frames[j]->payloadLength;
    }

    char payload[payload_length+1];
    payload[payload_length] = '\0';

    for (j = 0; likely(j < frame_count); j++) {
        memcpy(payload+current_length, frames[j]->payload, frames[j]->payloadLength);
        current_length += frames[j]->payloadLength;
    }

    // https://github.com/madler/zlib/issues/149
    if (comp->params.server_no_context_takeover) {
        flush_mask = Z_BLOCK;
    }

    // Compress whole message
    comp->compressor.avail_in = payload_length;
    comp->compressor.next_in = (unsigned char *)payload;

    do {
        if ( unlikely(NULL == (message = allocs.realloc(message, (message_length+default_chunk_size+2)*sizeof(char)))) ) {
            allocs.free(message);
            return;
        }
        memset(message+message_length, '\0', default_chunk_size+2);

        comp->compressor.avail_out = default_chunk_size;
        comp->compressor.next_out = (unsigned char *)message+message_length;
        switch (deflate(&comp->compressor, flush_mask)) {
            case Z_OK:
            case Z_STREAM_END:
            case Z_BUF_ERROR:
                break;
            default:
                allocs.free(message);
                return;
        }
        message_length += default_chunk_size - comp->compressor.avail_out;
    } while ( comp->compressor.avail_out == 0 );

    // https://github.com/madler/zlib/issues/149
    if (comp->params.server_no_context_takeover) {
        flush_mask = Z_FULL_FLUSH;

        if ( unlikely(NULL == (message = allocs.realloc(message, (message_length+default_chunk_size+2)*sizeof(char)))) ) {
            allocs.free(message);
            return;
        }
        memset(message+message_length, '\0', default_chunk_size+2);

        comp->compressor.avail_out = default_chunk_size;
        comp->compressor.next_out = (unsigned char *)message+message_length;
        switch (deflate(&comp->compressor, flush_mask)) {
            case Z_OK:
            case Z_STREAM_END:
            case Z_BUF_ERROR:
                break;
            default:
                allocs.free(message);
                return;
        }
        message_length += default_chunk_size - comp->compressor.avail_out;
    }

    if ( unlikely(message_length < 5 || memcmp(message+message_length-4, "\x00\x00\xff\xff", 4) != 0) ) {
        message[message_length] = '\x00';
        message_length++;
    } else {
        memset(message+message_length-4, '\0', 4);
        message_length -= 4;
    }

    // set rsv1 bit
    frames[0]->rsv1 = true;

    current_length = 0;

    // Reallocate application data to contain the compressed data in the same
    // amount of frames
    for (j = 0; likely(j < frame_count); j++) {
        if ( likely(j+1 != frame_count) ) {
            size = message_length/frame_count;
        } else {
            size = message_length-current_length;
        }

        if ( unlikely(NULL == (frames[j]->payload = allocs.realloc(frames[j]->payload, size))) ) {
            allocs.free(message);
            return;
        }
        memcpy(frames[j]->payload, message+current_length, size);
        current_length += size;
        frames[j]->extensionDataLength = 0;
        frames[j]->applicationDataLength = size;
        frames[j]->payloadLength = size;
    }

    allocs.free(message);
    deflateReset(&comp->compressor);
}
*/

void outFrames(int fd, wss_frame_t **frames, size_t frames_count) {
    wss_frame_t *frame;
    size_t j, total_in;
    size_t payload_length    = 0;
    size_t current_length    = 0;
    size_t next_start_index  = 0;
    size_t last_frame_length = 0;
    size_t last_index        = frames_count-1;
    int flush_mask = Z_SYNC_FLUSH;
    char buffer[default_chunk_size];
    wss_comp_t *comp;

    if ( unlikely(frames[0]->opcode >= 0x8 && frames[0]->opcode <= 0xA) ) {
        return;
    }

    if ( unlikely(pthread_mutex_lock(&lock) != 0) ) {
        return;
    }

    HASH_FIND_INT(compressors, &fd, comp);

    pthread_mutex_unlock(&lock);

    if ( unlikely(NULL == comp) ) {
        return;
    }

    for (j = 0; likely(j < frames_count); j++) {
        payload_length += frames[j]->payloadLength; 
    }

    char payload[payload_length+1];
    payload[payload_length] = '\0';

    for (j = 0; likely(j < frames_count); j++) {
        memcpy(payload+current_length, frames[j]->payload, frames[j]->payloadLength);
        current_length += frames[j]->payloadLength;
    }

    // https://github.com/madler/zlib/issues/149
    if (comp->params.server_no_context_takeover) {
        flush_mask = Z_BLOCK;
    }

    // Compress whole message
    comp->compressor.avail_in = payload_length;
    comp->compressor.next_in = (unsigned char *)payload;

    total_in = comp->compressor.total_in;
    do {
        comp->compressor.avail_out = default_chunk_size;
        comp->compressor.next_out = (unsigned char *)buffer;
        switch (deflate(&comp->compressor, flush_mask)) {
            case Z_OK:
            case Z_STREAM_END:
            case Z_BUF_ERROR:
                break;
            default:
                return;
        }

        if ( unlikely(! writeFrames(frames, &comp->compressor, buffer, &last_frame_length, &next_start_index, total_in, payload_length, last_index)) ) {
            return;
        }
    } while ( comp->compressor.avail_out == 0 );

    // https://github.com/madler/zlib/issues/149
    if (comp->params.server_no_context_takeover) {
        flush_mask = Z_FULL_FLUSH;

        comp->compressor.avail_out = default_chunk_size;
        comp->compressor.next_out = (unsigned char *)buffer;
        switch (deflate(&comp->compressor, flush_mask)) {
            case Z_OK:
            case Z_STREAM_END:
            case Z_BUF_ERROR:
                break;
            default:
                return;
        }

        if ( unlikely(! writeFrames(frames, &comp->compressor, buffer, &last_frame_length, &next_start_index, total_in, payload_length, last_index)) ) {
            return;
        }
    }

    // Set last frame length
    frame = frames[last_index];
    frame->extensionDataLength = 0;
    frame->applicationDataLength = last_frame_length;
    frame->payloadLength = frames[last_index]->applicationDataLength + frames[last_index]->extensionDataLength;

    // Remove extra bytes from last frame
    if ( unlikely(frame->payloadLength < 5 || memcmp(frame->payload+(frame->payloadLength-4), "\x00\x00\xff\xff", 4) != 0) ) {
        frame->payload[frame->payloadLength] = '\x00';
        frame->payloadLength += 1;
        frame->applicationDataLength += 1;
    } else {
        memset(frame->payload+(frame->payloadLength-4), '\x00', 4);
        frame->payloadLength -= 4;
        frame->applicationDataLength -= 4;
    }

    // set rsv1 bit
    frames[0]->rsv1 = 1;

    // TODO: should we reset?
    deflateReset(&comp->compressor);
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

        WSS_UNUSED(inflateEnd(&comp->decompressor));
        WSS_UNUSED(deflateEnd(&comp->compressor));

        allocs.free(comp);
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

            allocs.free(comp);
        }
    }

    pthread_mutex_unlock(&lock);
    pthread_mutex_destroy(&lock);
}
