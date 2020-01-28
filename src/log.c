#include <stdio.h> 				/* printf, fflush, fprintf, fopen, fclose */
#include <time.h> 				/* time_t, struct *tm, time, localtime */
#include <pthread.h> 			/* pthread_self */

#include "server.h"
#include "log.h"
#include "predict.h"

log_t log_type;

/**
 * Function that logs whatever is given to it.
 *
 * @param   type    [log_t]     "The type of logging"
 * @param   msg     [char *]    "The error message"
 * @param   name    [char *]    "The name of the file where the log came from"
 * @param   line    [int]       "The linenumber that the log was called"
 * @return          [void]
 */
void WSS_log(log_t type, char *msg, char *name, int line) {
    FILE *file;
    time_t rawtime;
    struct tm *timeinfo;
    int n;
    char t[6];

    if ( unlikely((log_type & type) == 0) ) {
        return;
    }

    switch (type) {
        case SERVER_ERROR:
            sprintf(t, "%s", "error");
            if ( NULL == (file = fopen("log/server.log", "a+")) ) {
                fclose(file);
                return;
            }
            break;
        case SERVER_TRACE:
            sprintf(t, "%s", "trace");
            if ( NULL == (file = fopen("log/server.log", "a+")) ) {
                fclose(file);
                return;
            }
            break;
        case SERVER_DEBUG:
            sprintf(t, "%s", "debug");
            if ( NULL == (file = fopen("log/server.log", "a+")) ) {
                fclose(file);
                return;
            }
            break;
        case CLIENT_ERROR:
            sprintf(t, "%s", "error");
            if ( NULL == (file = fopen("log/client.log", "a+")) ) {
                fclose(file);
                return;
            }
            break;
        case CLIENT_TRACE:
            sprintf(t, "%s", "trace");
            if ( NULL == (file = fopen("log/client.log", "a+")) ) {
                fclose(file);
                return;
            }
            break;
        case CLIENT_DEBUG:
            sprintf(t, "%s", "debug");
            if ( NULL == (file = fopen("log/client.log", "a+")) ) {
                fclose(file);
                return;
            }
            break;
        default:
            return;
    }

    if ( unlikely((time(&rawtime) == (time_t) -1)) ) {
        return;
    }

    if ( unlikely(NULL == (timeinfo = localtime(&rawtime))) ) {
        return;
    }

    n = fprintf(file, "[%s] {#%d} %s \t - %s ~ l. %d\n\t - %s\n", t, (int)pthread_self(),
            asctime(timeinfo), name, line, msg);

    if ( unlikely(n < 0) ) {
        return;
    }

    fclose(file);
}
