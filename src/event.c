#include "event.h" 

int close_pipefd[2] = {-1, -1};

/**
 * Function that adds function to threadpools task queue.
 *
 * @param 	pool	[threadpool_t *] 	"A threadpool instance"
 * @param 	func	[void (*)(void *)] 	"A function pointer to add to task queue"
 * @param 	args	[void *] 	        "Arguments to be served to the function"
 * @return 			[wss_error_t]       "The error status"
 */
wss_error_t WSS_poll_add_task_to_threadpool(threadpool_t *pool, void (*func)(void *), void *args) {
    int err;

    do {
        if ( unlikely((err = threadpool_add(pool, func, args, 0)) != 0) ) {
            switch (err) {
                case threadpool_invalid:
                    WSS_log_fatal("Threadpool was served with invalid data");
                    return WSS_THREADPOOL_INVALID_ERROR;
                case threadpool_lock_failure:
                    WSS_log_fatal("Locking in thread failed");
                    return WSS_THREADPOOL_LOCK_ERROR;
                case threadpool_queue_full:
                    // Currently we treat a full threadpool as an error, but
                    // we could try to handle this by dynamically increasing size
                    // of threadpool, and maybe reset thread count to that of the
                    // configuration when the hot load is over
                    WSS_log_error("Threadpool queue is full");
                    return WSS_THREADPOOL_FULL_ERROR;
                case threadpool_shutdown:
                    WSS_log_error("Threadpool is shutting down");
                    return WSS_THREADPOOL_SHUTDOWN_ERROR;
                case threadpool_thread_failure:
                    WSS_log_fatal("Threadpool thread return an error");
                    return WSS_THREADPOOL_THREAD_ERROR;
                default:
                    WSS_log_fatal("Unknown error occured with threadpool");
                    return WSS_THREADPOOL_ERROR;
            }
        }
    } while (0);

    return WSS_SUCCESS;
}
