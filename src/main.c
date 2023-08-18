#include <stdio.h> 				/* fprintf */
#include <stdlib.h>             /* NULL */
#include <getopt.h> 			/* getopt_long, struct option */
#include <string.h> 			/* strerror */
#include <errno.h> 			    /* errno */

#include "server.h"             /* server_start */
#include "config.h"             /* config_t, WSS_config_load(), WSS_config_free() */
#include "error.h"              /* wss_error_t */
#include "subprotocols.h"       /* wss_subprotocol_t */
#include "alloc.h"              /* WSS_malloc() */
#include "log.h"                /* WSS_log_*() */
#include "core.h"

static void log_mutex(void *udata, int lock) {
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

static inline void wss_help(FILE *stream) {
    fprintf(
            stream,
            "Usage: ./WSServer [OPTIONS]\n\n"
            "OPTIONS:\n"
            "\t-h             | --help                 show server arguments\n"
            "\t-c CONFIG_PATH | --config CONFIG_PATH   provide JSON configuration file\n"
           );
}

int main(int argc, char *argv[]) {
    int err, res;
    wss_config_t config;
    FILE *file;
    pthread_mutex_t log_lock;
    char *echo = "subprotocols/echo/echo.so";
    char *broadcast = "subprotocols/broadcast/broadcast.so";

#ifdef USE_RPMALLOC
    rpmalloc_initialize();
#endif

    static struct option long_options[] = {
        {"help"   	    , no_argument,       0, 'h'},
        {"config"       , required_argument, 0, 'c'},
        {0          	, 0                , 0,  0 }
    };
    
    // Default values
    config.subprotocols         = NULL;
    config.subprotocols_config  = NULL;
    config.subprotocols_length  = 0;
    config.extensions           = NULL;
    config.extensions_length    = 0;
    config.extensions_config    = NULL;
    config.favicon              = NULL;
    config.origins              = NULL;
    config.origins_length       = 0;
    config.hosts                = NULL;
    config.hosts_length         = 0;
    config.paths                = NULL;
    config.paths_length         = 0;
    config.queries              = NULL;
    config.queries_length       = 0;
    config.string               = NULL;
    config.data                 = NULL;
    config.length               = 0;
    config.port_http 	        = 80;
    config.port_https	        = 443;
#ifdef NDEBUG
    config.log 		            = WSS_LOG_INFO;
#else
    config.log 		            = WSS_LOG_TRACE;
#endif
    config.size_header          = 8192;
    config.size_payload         = 16777215;
    config.size_uri 	        = 8192;
    config.size_ringbuffer      = 128;
    config.size_buffer          = 32768;
    config.size_thread          = 2097152;
    config.size_frame           = 1048576;
    config.max_frames           = 1048576;
    config.pool_io_workers      = 3;
    config.pool_io_tasks        = 1024;
    config.pool_connect_workers = 1;
    config.pool_connect_tasks   = 1024;
    config.timeout_pings        = 1;     // Times that a client will be pinged before timeout occurs
    config.timeout_poll         = -1;    // Infinite
    config.timeout_read         = 1000;  // 1 Second
    config.timeout_write        = 1000;  // 1 Second
    config.timeout_client       = 60000; // 1 Hour
    config.ssl_key              = NULL;
    config.ssl_cert             = NULL;
    config.ssl_dhparam          = NULL;
    config.ssl_ca_file          = NULL;
    config.ssl_ca_path          = NULL;
    config.ssl_cipher_list      = NULL;
    config.ssl_cipher_suites    = NULL;
    config.ssl_compression      = false;
    config.ssl_peer_cert        = false;

    if ( NULL == (file = fopen("log/WSServer.log", "a+")) ) {
        WSS_config_free(&config);
        fprintf(stderr, "%s\n", strerror(errno));
#ifdef USE_RPMALLOC
        rpmalloc_finalize();
#endif
        return EXIT_FAILURE;
    }

    // Set file to write log to
    log_set_fp(file);

    // Set log level to default value until configuration is loaded
    log_set_level(config.log);

    if ( unlikely((err = pthread_mutex_init(&log_lock, NULL)) != 0) ) {
        WSS_config_free(&config);
        WSS_log_error("Unable to initialize log lock: %s", strerror(err));
        fclose(file);
#ifdef USE_RPMALLOC
        rpmalloc_finalize();
#endif
        return EXIT_FAILURE;
    }

    // Set lock to use in interal lock function
    log_set_udata(&log_lock);

    // Set lock function for logging functions
    log_set_lock(log_mutex);

    while ( likely((err = getopt_long(argc, argv, "c:h", long_options, NULL)) != -1)) {
        switch (err) {
            case 'c':
                if ( unlikely(WSS_SUCCESS != WSS_config_load(&config, optarg)) ) {
                    WSS_config_free(&config);

                    if ( unlikely((err = pthread_mutex_destroy(&log_lock)) != 0) ) {
                        WSS_log_error("Unable to initialize log lock: %s", strerror(err));
                    }

                    fclose(file);
#ifdef USE_RPMALLOC
                    rpmalloc_finalize();
#endif
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
                wss_help(stdout);
                WSS_config_free(&config);
                if ( unlikely((err = pthread_mutex_destroy(&log_lock)) != 0) ) {
                    WSS_log_error("Unable to initialize log lock: %s", strerror(err));
                    fclose(file);
#ifdef USE_RPMALLOC
                    rpmalloc_finalize();
#endif
                    return EXIT_FAILURE;
                }
                fclose(file);
#ifdef USE_RPMALLOC
                rpmalloc_finalize();
#endif
                return EXIT_SUCCESS;
            default:
                wss_help(stderr);
                WSS_config_free(&config);
                if ( unlikely((err = pthread_mutex_destroy(&log_lock)) != 0) ) {
                    WSS_log_error("Unable to initialize log lock: %s", strerror(err));
                }
                fclose(file);
#ifdef USE_RPMALLOC
                rpmalloc_finalize();
#endif
                return EXIT_FAILURE;
        }
    }

    WSS_log_info("Pool io workers %d", config.pool_io_workers);
    WSS_log_info("Pool io tasks %d", config.pool_io_tasks);
    WSS_log_info("Pool connect workers %d", config.pool_connect_workers);
    WSS_log_info("Pool connect tasks %d", config.pool_connect_tasks);

    // If in production mode do not print to stdout
#ifdef NDEBUG
    WSS_log_info("Log is in quiet mode");
    log_set_quiet(1);
#endif

    // Set log level to what what specified in the configuration
    log_set_level(config.log);

    // Setting echo and broadcast protocols as default if none was loaded
    if ( config.subprotocols_length == 0 && config.subprotocols == NULL) {
        if ( NULL != (config.subprotocols = WSS_calloc(2, sizeof(char *)))) {
            if ( NULL != (config.subprotocols_config = WSS_calloc(2, sizeof(char *)))) {
                config.subprotocols[0] = echo;
                config.subprotocols[1] = broadcast;
                config.subprotocols_config[0] = NULL;
                config.subprotocols_config[1] = NULL;
                config.subprotocols_length = 2;
                config.subprotocols_default = 0;
            }
        }
    }

    res = WSS_server_start(&config);

    WSS_log_trace("Server stopped");

    if ( unlikely((err = pthread_mutex_destroy(&log_lock)) != 0) ) {
        WSS_log_error("Unable to initialize log lock: %s", strerror(err));
        res = EXIT_FAILURE;
    }

    WSS_log_trace("Log mutex destoyed");

    if ( unlikely(WSS_SUCCESS != WSS_config_free(&config)) ) {
        res = EXIT_FAILURE;
    }

    fclose(file);

    WSS_log_trace("Config freed");

#ifdef USE_RPMALLOC
    rpmalloc_finalize();
    WSS_log_trace("rpmalloc finalized");
#endif

    return res;
}
