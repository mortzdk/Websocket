/*
 * Copyright (c) 2016-2017 Mindaugas Rasiukevicius <rmind at noxt eu>
 * All rights reserved.
 *
 * Use is subject to license terms, as specified in the LICENSE file.
 */

/*
 * Atomic multi-producer single-consumer ring buffer, which supports
 * contiguous range operations and which can be conveniently used for
 * message passing.
 *
 * There are three offsets -- think of clock hands:
 * - NEXT: marks the beginning of the available space,
 * - WRITTEN: the point up to which the data is actually written.
 * - Observed READY: point up to which data is ready to be written.
 *
 * Producers
 *
 *	Observe and save the 'next' offset, then request N bytes from
 *	the ring buffer by atomically advancing the 'next' offset.  Once
 *	the data is written into the "reserved" buffer space, the thread
 *	clears the saved value; these observed values are used to compute
 *	the 'ready' offset.
 *
 * Consumer
 *
 *	Writes the data between 'written' and 'ready' offsets and updates
 *	the 'written' value.  The consumer thread scans for the lowest
 *	seen value by the producers.
 *
 * Key invariant
 *
 *	Producers cannot go beyond the 'written' offset; producers are
 *	also not allowed to catch up with the consumer.  Only the consumer
 *	is allowed to catch up with the producer i.e. set the 'written'
 *	offset to be equal to the 'next' offset.
 *
 * Wrap-around
 *
 *	If the producer cannot acquire the requested length due to little
 *	available space at the end of the buffer, then it will wraparound.
 *	WRAP_LOCK_BIT in 'next' offset is used to lock the 'end' offset.
 *
 *	There is an ABA problem if one producer stalls while a pair of
 *	producer and consumer would both successfully wrap-around and set
 *	the 'next' offset to the stale value of the first producer, thus
 *	letting it to perform a successful CAS violating the invariant.
 *	A counter in the 'next' offset (masked by WRAP_COUNTER) is used
 *	to prevent from this problem.  It is incremented on wraparounds.
 *
 *	The same ABA problem could also cause a stale 'ready' offset,
 *	which could be observed by the consumer.  We set WRAP_LOCK_BIT in
 *	the 'seen' value before advancing the 'next' and clear this bit
 *	after the successful advancing; this ensures that only the stable
 *	'ready' observed by the consumer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "ringbuf.h"
#include "ringbuf_utils.h"

#define	RBUF_OFF_MASK	(0x00000000ffffffffUL)
#define	WRAP_LOCK_BIT	(0x8000000000000000UL)
#define	RBUF_OFF_MAX	(UINT64_MAX & ~WRAP_LOCK_BIT)

#define	WRAP_COUNTER	(0x7fffffff00000000UL)
#define	WRAP_INCR(x)	(((x) + 0x100000000UL) & WRAP_COUNTER)

typedef uint64_t	ringbuf_off_t;
typedef uint64_t	worker_off_t;
typedef uint64_t	registered_t;

enum
{
	not_registered,
	being_registered,	/* Being registered in register_worker() */
	perm_registered,	/* Registered in ringbuf_register() */
	temp_registered		/* Registered in ringbuf_acquire() */
};

struct ringbuf_worker {
	volatile ringbuf_off_t	seen_off;
	registered_t		registered;
};

struct ringbuf {
	/* Ring buffer space. */
	size_t			space;

	/*
	 * The NEXT hand is atomically updated by the producer.
	 * WRAP_LOCK_BIT is set in case of wrap-around; in such case,
	 * the producer can update the 'end' offset.
	 */
	volatile ringbuf_off_t	next;
	ringbuf_off_t		end;

	/* The index of the first potentially free worker-record. */
	worker_off_t		first_free_worker;

	/* The following are updated by the consumer. */
	ringbuf_off_t		written;
	unsigned		nworkers, ntempworkers;
	ringbuf_worker_t	workers[];
};

/*
 * ringbuf_setup: initialise a new ring buffer of a given length.
 */
int
ringbuf_setup(ringbuf_t *rbuf, unsigned nworkers, unsigned ntempworkers, size_t length)
{
	if (length >= RBUF_OFF_MASK) {
		errno = EINVAL;
		return -1;
	}
	memset(rbuf, 0, offsetof(ringbuf_t, workers[nworkers + ntempworkers]));
	rbuf->space = length;
	rbuf->end = RBUF_OFF_MAX;
	rbuf->nworkers = nworkers;
	rbuf->ntempworkers = ntempworkers;
	return 0;
}

/*
 * ringbuf_get_sizes: return the sizes of the ringbuf_t and ringbuf_worker_t.
 */
void
ringbuf_get_sizes(unsigned nworkers, unsigned ntempworkers,
    size_t *ringbuf_size, size_t *ringbuf_worker_size)
{
	if (ringbuf_size)
		*ringbuf_size = offsetof(ringbuf_t, workers[nworkers + ntempworkers]);
	if (ringbuf_worker_size)
		*ringbuf_worker_size = sizeof(ringbuf_worker_t);
}

/*
 * register_worker: allocate a worker-record for a thread/process,
 * and pass back the pointer to its local store.
 * Returns NULL if none are available.
 */
static ringbuf_worker_t *
register_worker(ringbuf_t *rbuf, unsigned registration_type)
{
	worker_off_t volatile *p_free_worker;
	int acquired, state;
	ringbuf_worker_t *w = NULL;

	/* Try to find a worker-record that can be registered. */
	p_free_worker = &rbuf->first_free_worker;
	acquired = false;
	while (!acquired) {
		worker_off_t prev_free_worker, i;

		/* Get the index of the first worker-record to try registering. */
		prev_free_worker = *p_free_worker;

		for (i = 0; !acquired && i < rbuf->ntempworkers; ++i) {
			worker_off_t new_free_worker;

			/* Prepare to acquire a worker-record index. */
			new_free_worker = ((prev_free_worker & RBUF_OFF_MASK)
				+ i) % rbuf->ntempworkers;

			/* Try to acquire a worker-record. */
			w = &rbuf->workers[new_free_worker + rbuf->nworkers];
            state = not_registered;
			if (!atomic_compare_exchange_weak(&w->registered, &state, being_registered))
				continue;
			acquired = true;
			w->seen_off = RBUF_OFF_MAX;
			atomic_thread_fence(memory_order_release);
			w->registered = registration_type;

			/* Advance the index if no one else has. */
			new_free_worker |= WRAP_INCR(prev_free_worker);
			atomic_compare_exchange_weak(p_free_worker, &prev_free_worker, new_free_worker);
		}

		/*
		 * If no worker-record could be registered, and no one else was
		 * trying to register at the same time, then stop searching.
		 */
		if (!acquired && (*p_free_worker) == prev_free_worker)
			break;
	}

	/* Register this worker-record. */
	return w;
}

/*
 * ringbuf_register: register the worker (thread/process) as a producer
 * and pass the pointer to its local store.
 */
ringbuf_worker_t *
ringbuf_register(ringbuf_t *rbuf, unsigned i)
{
	ASSERT (i < rbuf->nworkers);

	ringbuf_worker_t *w = &rbuf->workers[i];

	w->seen_off = RBUF_OFF_MAX;
	atomic_thread_fence(memory_order_release);
	w->registered = perm_registered;
	return w;
}

void
ringbuf_unregister(ringbuf_t *rbuf, ringbuf_worker_t *w)
{
	w->registered = not_registered;
	(void)rbuf;
}

/*
 * stable_nextoff: capture and return a stable value of the 'next' offset.
 */
static inline ringbuf_off_t
stable_nextoff(ringbuf_t *rbuf)
{
	unsigned count = SPINLOCK_BACKOFF_MIN;
	ringbuf_off_t next;

	while ((next = rbuf->next) & WRAP_LOCK_BIT) {
		SPINLOCK_BACKOFF(count);
	}
	atomic_thread_fence(memory_order_acquire);
	ASSERT((next & RBUF_OFF_MASK) < rbuf->space);
	return next;
}

/*
 * ringbuf_acquire: request a space of a given length in the ring buffer.
 *
 * => On success: returns the offset at which the space is available.
 * => On failure: returns -1.
 */
ssize_t
ringbuf_acquire(ringbuf_t *rbuf, ringbuf_worker_t **pw, size_t len)
{
	ringbuf_off_t seen, next, target;
	ringbuf_worker_t *w;

	ASSERT(len > 0 && len <= rbuf->space);

	/* If necessary, acquire a worker-record. */
	if (*pw == NULL) {
		w = register_worker(rbuf, temp_registered);
		if (w == NULL)
			return -1;
		*pw = w;
	} else {
		w = *pw;
	}
	ASSERT(w->seen_off == RBUF_OFF_MAX);

	do {
		ringbuf_off_t written;

		/*
		 * Get the stable 'next' offset.  Save the observed 'next'
		 * value (i.e. the 'seen' offset), but mark the value as
		 * unstable (set WRAP_LOCK_BIT).
		 *
		 * Note: CAS will issue a memory_order_release for us and
		 * thus ensures that it reaches global visibility together
		 * with new 'next'.
		 */
		seen = stable_nextoff(rbuf);
		next = seen & RBUF_OFF_MASK;
		ASSERT(next < rbuf->space);
		w->seen_off = next | WRAP_LOCK_BIT;

		/*
		 * Compute the target offset.  Key invariant: we cannot
		 * go beyond the WRITTEN offset or catch up with it.
		 */
		target = next + len;
		written = rbuf->written;
		if (__predict_false(next < written && target >= written)) {
			/* The producer must wait. */
			w->seen_off = RBUF_OFF_MAX;
			if (w->registered == temp_registered) {
				*pw = NULL;
				atomic_thread_fence(memory_order_release);
				w->registered = not_registered;
			}
			return -1;
		}

		if (__predict_false(target >= rbuf->space)) {
			const bool exceed = target > rbuf->space;

			/*
			 * Wrap-around and start from the beginning.
			 *
			 * If we would exceed the buffer, then attempt to
			 * acquire the WRAP_LOCK_BIT and use the space in
			 * the beginning.  If we used all space exactly to
			 * the end, then reset to 0.
			 *
			 * Check the invariant again.
			 */
			target = exceed ? (WRAP_LOCK_BIT | len) : 0;
			if ((target & RBUF_OFF_MASK) >= written) {
				w->seen_off = RBUF_OFF_MAX;
				if (w->registered == temp_registered) {
					*pw = NULL;
					atomic_thread_fence(memory_order_release);
					w->registered = not_registered;
				}
				return -1;
			}
			/* Increment the wrap-around counter. */
			target |= WRAP_INCR(seen & WRAP_COUNTER);
		} else {
			/* Preserve the wrap-around counter. */
			target |= seen & WRAP_COUNTER;
		}
	} while (!atomic_compare_exchange_weak(&rbuf->next, &seen, target));

	/*
	 * Acquired the range.  Clear WRAP_LOCK_BIT in the 'seen' value
	 * thus indicating that it is stable now.
	 */
	w->seen_off &= ~WRAP_LOCK_BIT;

	/*
	 * If we set the WRAP_LOCK_BIT in the 'next' (because we exceed
	 * the remaining space and need to wrap-around), then save the
	 * 'end' offset and release the lock.
	 */
	if (__predict_false(target & WRAP_LOCK_BIT)) {
		/* Cannot wrap-around again if consumer did not catch-up. */
		ASSERT(rbuf->written <= next);
		ASSERT(rbuf->end == RBUF_OFF_MAX);
		rbuf->end = next;
		next = 0;

		/*
		 * Unlock: ensure the 'end' offset reaches global
		 * visibility before the lock is released.
		 */
		atomic_thread_fence(memory_order_release);
		rbuf->next = (target & ~WRAP_LOCK_BIT);
	}
	ASSERT((target & RBUF_OFF_MASK) <= rbuf->space);
	return (ssize_t)next;
}

/*
 * ringbuf_produce: indicate the acquired range in the buffer is produced
 * and is ready to be consumed.
 */
void
ringbuf_produce(ringbuf_t *rbuf, ringbuf_worker_t **pw)
{
	ringbuf_worker_t *w = *pw;

	(void)rbuf;
	ASSERT(w->registered != not_registered
		&& w->registered != being_registered);
	ASSERT(w->seen_off != RBUF_OFF_MAX);
	atomic_thread_fence(memory_order_release);
	w->seen_off = RBUF_OFF_MAX;

	/* Free any temporarily-allocated worker-record. */
	if (w->registered == temp_registered) {
		w->registered = not_registered;
		*pw = NULL;
	}
}

/*
 * ringbuf_consume: get a contiguous range which is ready to be consumed.
 */
size_t
ringbuf_consume(ringbuf_t *rbuf, size_t *offset)
{
	ringbuf_off_t written = rbuf->written, next, ready;
	size_t towrite;
	unsigned total_workers;
retry:
	/*
	 * Get the stable 'next' offset.  Note: stable_nextoff() issued
	 * a load memory barrier.  The area between the 'written' offset
	 * and the 'next' offset will be the *preliminary* target buffer
	 * area to be consumed.
	 */
	next = stable_nextoff(rbuf) & RBUF_OFF_MASK;
	if (written == next) {
		/* If producers did not advance, then nothing to do. */
		return 0;
	}

	/*
	 * Observe the 'ready' offset of each producer.
	 *
	 * At this point, some producer might have already triggered the
	 * wrap-around and some (or all) seen 'ready' values might be in
	 * the range between 0 and 'written'.  We have to skip them.
	 */
	ready = RBUF_OFF_MAX;

	total_workers = rbuf->nworkers + rbuf->ntempworkers;
	for (unsigned i = 0; i < total_workers; i++) {
		ringbuf_worker_t *w = &rbuf->workers[i];
		unsigned count = SPINLOCK_BACKOFF_MIN;
		ringbuf_off_t seen_off;

		/* Skip if the worker has not registered. */
		if (w->registered == not_registered
				|| w->registered == being_registered) {
			continue;
		}

		/*
		 * Get a stable 'seen' value.  This is necessary since we
		 * want to discard the stale 'seen' values.
		 */
		while ((seen_off = w->seen_off) & WRAP_LOCK_BIT) {
			SPINLOCK_BACKOFF(count);
		}

		/*
		 * Ignore the offsets after the possible wrap-around.
		 * We are interested in the smallest seen offset that is
		 * not behind the 'written' offset.
		 */
		if (seen_off >= written) {
			ready = MIN(seen_off, ready);
		}
		ASSERT(ready >= written);
	}

	/*
	 * Finally, we need to determine whether wrap-around occurred
	 * and deduct the safe 'ready' offset.
	 */
	if (next < written) {
		const ringbuf_off_t end = MIN(rbuf->space, rbuf->end);

		/*
		 * Wrap-around case.  Check for the cut off first.
		 *
		 * Reset the 'written' offset if it reached the end of
		 * the buffer or the 'end' offset (if set by a producer).
		 * However, we must check that the producer is actually
		 * done (the observed 'ready' offsets are clear).
		 */
		if (ready == RBUF_OFF_MAX && written == end) {
			/*
			 * Clear the 'end' offset if was set.
			 */
			if (rbuf->end != RBUF_OFF_MAX) {
				rbuf->end = RBUF_OFF_MAX;
				atomic_thread_fence(memory_order_release);
			}
			/* Wrap-around the consumer and start from zero. */
			rbuf->written = written = 0;
			goto retry;
		}

		/*
		 * We cannot wrap-around yet; there is data to consume at
		 * the end.  The ready range is smallest of the observed
		 * 'ready' or the 'end' offset.  If neither is set, then
		 * the actual end of the buffer.
		 */
		ASSERT(ready > next);
		ready = MIN(ready, end);
		ASSERT(ready >= written);
	} else {
		/*
		 * Regular case.  Up to the observed 'ready' (if set)
		 * or the 'next' offset.
		 */
		ready = MIN(ready, next);
	}
	towrite = ready - written;
	*offset = written;

	ASSERT(ready >= written);
	ASSERT(towrite <= rbuf->space);
	return towrite;
}

/*
 * ringbuf_release: indicate that the consumed range can now be released.
 */
void
ringbuf_release(ringbuf_t *rbuf, size_t nbytes)
{
	const size_t nwritten = rbuf->written + nbytes;

	ASSERT(rbuf->written <= rbuf->space);
	ASSERT(rbuf->written <= rbuf->end);
	ASSERT(nwritten <= rbuf->space);

	rbuf->written = (nwritten == rbuf->space) ? 0 : nwritten;
}