#include "message.h"
#include "server.h"
#include "worker.h"
#include "ringbuf.h"
#include "session.h"
#include "event.h"
#include "str.h"
#include "log.h"
#include "alloc.h"
#include "error.h"
#include "core.h"

#include <time.h>
#include <math.h>

void WSS_message_send_frames(void *serv, void *sess, wss_frame_t **frames, size_t frames_count) {
    size_t j;
    ssize_t off;
    char *out;
    uint64_t out_length;
    wss_message_t *m;
    ringbuf_worker_t *w = NULL;
    struct timespec tim;
    wss_server_t *server = (wss_server_t *)serv;
    wss_session_t *session = (wss_session_t *)sess;

    // Use extensions
    if ( NULL != session->header.ws_extensions ) {
        for (j = 0; likely(j < session->header.ws_extensions_count); j++) {
            session->header.ws_extensions[j]->ext->outframes(
                    session->fd,
                    frames,
                    frames_count);
        }
    }

    if ( unlikely((out_length = WSS_stringify_frames(frames, frames_count, &out)) == 0) ) {
        WSS_log_error("Unable to convert frames to message");

        return;
    }

    m = WSS_memorypool_alloc(server->message_pool);
    m->msg = out;
    m->length = out_length;
    m->framed = 1;

    WSS_log_trace("Putting message into ringbuffer");

    if ( unlikely(-1 == (off = ringbuf_acquire(session->ringbuf, &w, 1))) ) {
        WSS_log_error("Failed to acquire space in ringbuffer");

        WSS_message_free(m);

        return;
    }

    session->messages[off] = m;
    ringbuf_produce(session->ringbuf, &w);
    tim.tv_sec = 0;
    tim.tv_nsec = 100000000;

    do {
        pthread_mutex_lock(&session->lock);

        switch (session->state) {
        case READING:
            pthread_mutex_unlock(&session->lock);
            nanosleep(&tim, NULL);
            break;
        case CLOSING:
            WSS_session_jobs_dec(session);
            pthread_mutex_unlock(&session->lock);
            return;
        case CONNECTING:
            pthread_mutex_unlock(&session->lock);
            nanosleep(&tim, NULL);
            break;
        case WRITING:
            WSS_session_jobs_dec(session);
            pthread_mutex_unlock(&session->lock);
            return;
        case IDLE:
            session->state = WRITING;
            WSS_write(server, session);
            WSS_session_jobs_dec(session);
            pthread_mutex_unlock(&session->lock);
            return;
        }
    } while (1);
}

void WSS_message_send(int fd, wss_opcode_t opcode, char *message, uint64_t message_length) {
    size_t k;
    wss_session_t *session;
    wss_server_t *server = &servers.http;

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
        WSS_log_error("Unable to find session to send message to");
        return;
    }

    WSS_session_jobs_inc(session);

    if (NULL != session->ssl && session->ssl_connected) {
        server = &servers.https;
    }

    WSS_log_trace("Creating frames");

    // Store frames array on stack 
    size_t frames_count; 
    size_t frames_length = WSS_MAX(1, (size_t)ceil((double)message_length/(double)server->config->size_frame));
    wss_frame_t *frames[frames_length];
    wss_frame_t **frames_ptr = &frames[0];

    for (k = 0; likely(k < frames_length); k++) {
        frames[k] = WSS_memorypool_alloc(server->frame_pool);
    }

    frames_count = WSS_create_frames(server->config, opcode, message, message_length, &frames_ptr, frames_length);
    if ( likely(frames_count > 0)) {
        WSS_message_send_frames(server, session, frames, frames_count);
    }

    for (k = 0; likely(k < frames_length); k++) {
        WSS_free_frame(frames[k]);
        WSS_memorypool_dealloc(server->frame_pool, frames[k]);
    }
}

void WSS_message_free(wss_message_t *msg) {
    if ( likely(NULL != msg) ) {
        if ( likely(NULL != msg->msg) ) {
            WSS_free((void **)&msg->msg); 
        }

        msg->length = 0;
        msg->msg    = NULL;
        msg->framed = 0;
    }
}
