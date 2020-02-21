#include <stdio.h> 				/* fprintf */
#include <stdlib.h>             /* NULL */
#include <getopt.h> 			/* getopt_long, struct option */

#ifndef uthash_malloc
#define uthash_malloc(sz) WSS_malloc(sz)      /* malloc fcn                      */
#endif

#ifndef uthash_free
#define uthash_free(ptr,sz) WSS_free(&ptr)     /* free fcn                        */
#endif

#include "server.h"             /* server_start */
#include "config.h"             /* config_t, config_load */
#include "error.h"              /* SUCCESS */
#include "alloc.h"              /* WSS_malloc */
#include "log.h"                /* WSS_malloc */
#include "predict.h"

void log_set_udata(void *udata);
void log_set_lock(log_LockFn fn);
void log_set_fp(FILE *fp);
void log_set_level(int level);
void log_set_quiet(int enable);

void log_mutex(void *udata, int lock) {
    int err;
    pthread_mutex_t *l = (pthread_mutex_t *) udata;
    if (lock) {
        if ( unlikely((err = pthread_mutex_lock(l)) != 0) ) {
            WSS_log_error("Unable to lock log lock: %s", strerror(err));
        }
    } else {
        if ( unlikely((err = pthread_mutex_unlock(l)) != 0) ) {
            WSS_log_error("Unable to unlock log lock: %s", strerror(err));
        }
    }
}

int main(int argc, char *argv[]) {
    int err, res;
    config_t config;
    FILE *file;
    pthread_mutex_t log_lock;

    static struct option long_options[] = {
        {"help"   	    , no_argument,       0, 'h'},
        {"log"   	    , required_argument, 0, 'l'},
        {"config"       , required_argument, 0, 'c'},
        {0          	, 0                , 0,  0 }
    };

    // Default values
    config.subprotocols         = NULL;
    config.subprotocols_length  = 0;
    config.extensions           = NULL;
    config.extensions_length    = 0;
    config.favicon              = NULL;
    config.ssl_ca_file          = NULL;
    config.ssl_ca_path          = NULL;
    config.ssl_key              = NULL;
    config.ssl_cert             = NULL;
    config.origins              = NULL;
    config.hosts                = NULL;;
    config.paths                = NULL;
    config.queries              = NULL;
    config.string               = NULL;
    config.data                 = NULL;
    config.length               = 0;
    config.port_http 	        = 9010;
    config.port_https	        = 9011;
#ifdef NDEBUG
    config.log 		            = WSS_LOG_INFO;
#else
    config.log 		            = WSS_LOG_TRACE;
#endif
    config.size_header          = 8192;
    config.size_payload         = 16777215;
    config.size_uri 	        = 8192;
    config.size_ringbuffer      = 128;
    config.size_buffer          = 1024;
    config.size_queue           = 1024;
    config.size_thread          = 524288;
    config.pool_workers         = 4;
    config.pool_queues          = 32;
    config.pool_size 	        = 8192;
    config.timeout              = 1000;
    config.origins_length       = 0;
    config.hosts_length         = 0;
    config.paths_length         = 0;
    config.queries_length       = 0;

    if ( NULL == (file = fopen("log/WSServer.log", "a+")) ) {
        fclose(file);
        return EXIT_FAILURE;
    }

    log_set_fp(file);
    log_set_level(config.log);

#ifdef NDEBUG
    log_set_quiet(1);
#endif

    if ( unlikely((err = pthread_mutex_init(&log_lock, NULL)) != 0) ) {
        WSS_log_error("Unable to initialize lock: %s", strerror(err));
        return EXIT_FAILURE;
    }

    log_set_udata(&log_lock);
    log_set_lock(log_mutex);

    while ( likely((err = getopt_long(argc, argv, "c:h", long_options, NULL)) != -1)) {
        switch (err) {
            case 'c':
                if ( unlikely(SUCCESS != config_load(&config, optarg)) ) {
                    config_free(&config);

                    return EXIT_FAILURE;
                }
                break;
            case 'h':
            default:
                fprintf(
                        stderr,
                        "Usage: ./WSServer [OPTIONS]\n\n"
                        "OPTIONS:\n"
                        "\t-h             | --help                 show server arguments\n"
                        "\t-c CONFIG_PATH | --config CONFIG_PATH   provide JSON configuration file\n"
                       );
                config_free(&config);
                return EXIT_FAILURE;
        }
    }

    log_set_level(config.log);

    res = server_start(&config);

    if ( unlikely(SUCCESS != config_free(&config)) ) {
        res = EXIT_FAILURE;
    }

    fclose(file);

    return res;
}
