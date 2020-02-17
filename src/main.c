#include <stdio.h> 				/* fprintf */
#include <stdlib.h>             /* NULL */
#include <getopt.h> 			/* getopt_long, struct option */

#include "server.h"             /* server_start */
#include "config.h"             /* config_t, config_load */
#include "error.h"              /* SUCCESS */
#include "alloc.h"              /* WSS_malloc */
#include "predict.h"

int main(int argc, char *argv[]) {
    int err, res;
    config_t config;

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
    config.log 		            = 0;
    config.size_header          = 8192;
    config.size_payload         = 16777215;
    config.size_uri 	        = 8192;
    config.size_pipe 	        = 128;
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

    while ( likely((err = getopt_long(argc, argv, "l:c:h", long_options, NULL)) != -1)) {
        switch (err) {
            case 'l':
                config.log = strtol(optarg, NULL, 10);
                break;
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
                        "\t-l LOG_VALUE   | --log LOG_VALUE        provide log value\n"
                        "\t-c CONFIG_PATH | --config CONFIG_PATH   provide JSON configuration file\n"
                       );
                config_free(&config);
                return EXIT_FAILURE;
        }
    }

    res = server_start(&config);

    if ( unlikely(SUCCESS != config_free(&config)) ) {
        res = EXIT_FAILURE;
    }

    return res;
}
