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
#include "predict.h"

#include <time.h>

void WSS_message_send_frames(void *serv, void *sess, wss_frame_t **frames, size_t frames_count) {
    size_t j, k;
    ssize_t off;
    char *out;
    uint64_t out_length;
    wss_message_t *m;
    ringbuf_worker_t *w = NULL;
    struct timespec tim;
    wss_server_t *server = (wss_server_t *)serv;
    wss_session_t *session = (wss_session_t *)sess;

    // Use extensions
    if ( NULL != session->header->ws_extensions ) {
        for (j = 0; likely(j < session->header->ws_extensions_count); j++) {
            session->header->ws_extensions[j]->ext->outframes(
                    session->fd,
                    frames,
                    frames_count);

            for (k = 0; likely(k < frames_count); k++) {
                session->header->ws_extensions[j]->ext->outframe(session->fd, frames[k]);
            }
        }
    }

    if ( unlikely((out_length = WSS_stringify_frames(frames, frames_count, &out)) == 0) ) {
        WSS_log_error("Unable to convert frames to message");

        return;
    }

    if ( unlikely(NULL == (m = WSS_malloc(sizeof(wss_message_t)))) ) {
        WSS_log_error("Unable to allocate message structure");

        WSS_free((void **) &out);

        return;
    }
    m->msg = out;
    m->length = out_length;
    m->framed = true;

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
    size_t frames_count;
    wss_session_t *session;
    wss_frame_t **frames;
    wss_server_t *server = servers.http;

    if ( unlikely(NULL == (session = WSS_session_find(fd))) ) {
        WSS_log_error("Unable to find session to send message to");
        return;
    }

    WSS_session_jobs_inc(session);

#ifdef USE_OPENSSL
    if (session->ssl && session->ssl_connected) {
        server = servers.https;
    }
#endif

    WSS_log_trace("Creating frames");

    frames_count = WSS_create_frames(server->config, opcode, message, message_length, &frames);

    WSS_message_send_frames(server, session, frames, frames_count);

    for (k = 0; likely(k < frames_count); k++) {
        WSS_free_frame(frames[k]);
    }
    WSS_free((void **) &frames);
}

void WSS_message_free(wss_message_t *msg) {
    if (NULL != msg) {
        if (NULL != msg->msg) {
            WSS_free((void **)&msg->msg); 
        }
        WSS_free((void **)&msg);
    }
}
