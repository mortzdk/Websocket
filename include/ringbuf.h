/*-
 * Copyright (c) 2016-2017 Mindaugas Rasiukevicius <rmind at noxt eu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _RINGBUF_H_
#define _RINGBUF_H_

__BEGIN_DECLS

typedef struct ringbuf ringbuf_t;
typedef struct ringbuf_worker ringbuf_worker_t;

int		ringbuf_setup(ringbuf_t *, unsigned, size_t);
void		ringbuf_get_sizes(unsigned, size_t *, size_t *);

ringbuf_worker_t *ringbuf_register(ringbuf_t *, unsigned);
void		ringbuf_unregister(ringbuf_t *, ringbuf_worker_t *);

ssize_t		ringbuf_acquire(ringbuf_t *, ringbuf_worker_t *, size_t);
void		ringbuf_produce(ringbuf_t *, ringbuf_worker_t *);
size_t		ringbuf_consume(ringbuf_t *, size_t *);
void		ringbuf_release(ringbuf_t *, size_t);

__END_DECLS

#endif
