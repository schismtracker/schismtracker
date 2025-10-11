/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "atomic.h"

#if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)

#include <stdatomic.h>

int atm_init(void) { return 0; }
void atm_quit(void) { }

int atm_cmpxchg(struct atm *atm, int32_t oldvalue, int32_t newvalue)
{
	return atomic_compare_exchange_weak((_Atomic volatile int32_t *)&atm->x, &oldvalue, newvalue);
}

int32_t atm_load(struct atm *atm)
{
	return atomic_load((const _Atomic volatile int32_t *)&atm->x);
}

void atm_store(struct atm *atm, int32_t x)
{
	atomic_store((_Atomic volatile int32_t *)&atm->x, x);
}

int atm_ptr_cmpxchg(struct atm_ptr *atm, void *oldvalue, void *newvalue)
{
	return atomic_compare_exchange_weak((void *_Atomic volatile *)&atm->x, &oldvalue, newvalue);
}

void *atm_ptr_load(struct atm_ptr *atm)
{
	return atomic_load((void *_Atomic volatile *)&atm->x);
}

void atm_ptr_store(struct atm_ptr *atm, void *x)
{
	atomic_store((void *_Atomic volatile *)&atm->x, x);
}

#elif !defined(USE_THREADS)

/* eh */

int atm_init(void) { return 0; }
void atm_quit(void) { }

int atm_cmpxchg(struct atm *atm, int32_t oldvalue, int32_t newvalue)
{
	if (atm->x != oldvalue)
		return 0;

	atm->x = newvalue;
	return 1;
}

int32_t atm_load(struct atm *atm)
{
	return atm->x;
}

void atm_store(struct atm *atm, int32_t x)
{
	atm->x = x;
}

void *atm_ptr_cmpxchg(struct atm *atm, void *oldvalue, void *newvalue)
{
	if (atm->x != oldvalue)
		return 0;

	atm->x = newvalue;
	return 1;
}

void *atm_ptr_load(struct atm_ptr *atm)
{
	return atm->x;
}

void atm_ptr_store(struct atm_ptr *atm, void *x)
{
	atm->x = x;
}

#elif SCHISM_GNUC_HAS_BUILTIN(__atomic_load, 4, 7, 0)

int atm_init(void) { return 0; }
void atm_quit(void) { }

int atm_cmpxchg(struct atm *atm, int32_t oldvalue, int32_t newvalue)
{
	return __atomic_compare_exchange(&atm->x, &oldvalue, &newvalue, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

int32_t atm_load(struct atm *atm)
{
	int32_t r;
	__atomic_load(&atm->x, &r, __ATOMIC_SEQ_CST);
	return r;
}

void atm_store(struct atm *atm, int32_t x)
{
	__atomic_store(&atm->x, &x, __ATOMIC_SEQ_CST);
}

int atm_ptr_cmpxchg(struct atm_ptr *atm, void *oldvalue, void *newvalue)
{
	return __atomic_compare_exchange(&atm->x, &oldvalue, &newvalue, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

void *atm_ptr_load(struct atm_ptr *atm)
{
	void *r;
	__atomic_load(&atm->x, &r, __ATOMIC_SEQ_CST);
	return r;
}

void atm_ptr_store(struct atm_ptr *atm, void *x)
{
	__atomic_store(&atm->x, &x, __ATOMIC_SEQ_CST);
}

#elif SCHISM_GNUC_HAS_BUILTIN(__sync_synchronize, 4, 1, 0)
/* I hope this is right */

int atm_init(void) { return 0; }
void atm_quit(void) { }

int atm_cmpxchg(struct atm *atm, int32_t oldvalue, int32_t newvalue)
{
	return __sync_bool_compare_and_swap(&atm->x, oldvalue, newvalue);
}

int32_t atm_load(struct atm *atm)
{
	__sync_synchronize();
	return atm->x;
}

void atm_store(struct atm *atm, int32_t x)
{
	atm->x = x;
	__sync_synchronize();
}

int atm_ptr_cmpxchg(struct atm_ptr *atm, void *oldvalue, void *newvalue)
{
	return __sync_bool_compare_and_swap(&atm->x, oldvalue, newvalue);
}

void *atm_ptr_load(struct atm_ptr *atm)
{
	__sync_synchronize();
	return atm->x;
}

void atm_ptr_store(struct atm_ptr *atm, void *x)
{
	atm->x = x;
	__sync_synchronize();
}

#elif defined(SCHISM_WIN32)
/* Interlocked* */

#include <windows.h>

SCHISM_STATIC_ASSERT(sizeof(LONG) == sizeof(int32_t), "LONG must be 32-bit");

int atm_init(void) { return 0; }
void atm_quit(void) { }

/* returns 1 if the exchange occurred, 0 if it didn't. */
int atm_cmpxchg(struct atm *atm, int32_t oldvalue, int32_t newvalue)
{
	return InterlockedCompareExchange((volatile LONG *)&atm->x, newvalue, oldvalue) == oldvalue;
}

int32_t atm_load(struct atm *atm)
{
	return InterlockedOr((volatile LONG *)&atm->x, 0);
}

void atm_store(struct atm *atm, int32_t x)
{
	InterlockedExchange((volatile LONG *)&atm->x, x);
}

int atm_ptr_cmpxchg(struct atm_ptr *atm, void *oldvalue, void *newvalue)
{
	return InterlockedCompareExchangePointer(&atm->x, newvalue, oldvalue) == oldvalue;
}

void *atm_ptr_load(struct atm_ptr *atm)
{
#if SIZEOF_VOID_P == 8
	return (void *)InterlockedOr64((volatile LONG64 *)&atm->x, 0);
#elif SIZEOF_VOID_P == 4
	return (void *)InterlockedOr((volatile LONG *)&atm->x, 0);
#else
# error what?
#endif
}

void atm_ptr_store(struct atm_ptr *atm, void *x)
{
#if SIZEOF_VOID_P == 8
	InterlockedExchange64((volatile LONG64 *)&atm->x, (LONG64)x);
#elif SIZEOF_VOID_P == 4
	InterlockedExchange((volatile LONG *)&atm->x, (LONG)x);
#else
# error what?
#endif
}

#elif defined(SCHISM_MACOSX)

# include <libkern/OSAtomic.h>

int atm_init(void) { return 0; }
void atm_quit(void) { }

int atm_cmpxchg(struct atm *atm, int32_t oldvalue, int32_t newvalue)
{
	return OSAtomicCompareAndSwap32Barrier(oldvalue, newvalue, &atm->x);
}

int32_t atm_load(struct atm *atm)
{
	return OSAtomicAdd32Barrier(0, &atm->x);
}

#define NEED_ATM_STORE

int atm_ptr_cmpxchg(struct atm *atm, void *oldvalue, void *newvalue)
{
#ifdef __LP64__
	return OSAtomicCompareAndSwap64Barrier((int64_t)oldvalue, (int64_t)newvalue, (volatile int64_t *)&atm->x);
#else
	return OSAtomicCompareAndSwap32Barrier((int32_t)oldvalue, (int32_t)newvalue, (volatile int32_t *)&atm->x);
#endif
}

void *atm_ptr_load(struct atm *atm)
{
#ifdef __LP64__
	return OSAtomicAdd64Barrier(0, (volatile int64_t *)&atm->x);
#else
	return OSAtomicAdd32Barrier(0, (volatile int32_t *)&atm->x);
#endif
}

#define NEED_ATM_PTR_STORE

#else
/* TODO: SDL has atomics, probably with more platforms than
 * we support now. We should be able to import it. */

#include "mt.h"

#define MUTEXES_SIZE (16)

static mt_mutex_t *mutexes[MUTEXES_SIZE] = {0};

int atm_init(void)
{
	uint32_t i;

	for (i = 0; i < MUTEXES_SIZE; i++) {
		mutexes[i] = mt_mutex_create();
		if (!mutexes[i])
			return -1;
	}

	/* at this point, the mutexes array should NEVER be touched again
	 * until we quit. */

	return 0;
}

void atm_quit(void)
{
	uint32_t i;

	for (i = 0; i < MUTEXES_SIZE; i++) {
		if (mutexes[i]) {
			mt_mutex_delete(mutexes[i]);
			mutexes[i] = NULL;
		}
	}
}

/* ------------------------------------------------------------------------ */

static inline SCHISM_ALWAYS_INLINE
mt_mutex_t *atm_get_mutex(struct atm *atm)
{
	/* TODO use alignof() here ... */
	return mutexes[((uintptr_t)atm / sizeof(*atm)) % MUTEXES_SIZE];
}

int atm_cmpxchg(struct atm *atm, int32_t oldvalue, int32_t newvalue)
{
	int r;
	mt_mutex_t *m = atm_get_mutex(atm);

	mt_mutex_lock(m);
	if (atm->x == oldvalue) {
		r = 1;
		atm->x = newvalue;
	} else {
		r = 0;
	}
	mt_mutex_unlock(m);

	return r;
}

#define NEED_ATM_LOAD
#define NEED_ATM_STORE

static inline SCHISM_ALWAYS_INLINE
mt_mutex_t *atm_ptr_get_mutex(struct atm_ptr *atm)
{
	/* TODO use alignof() here ... */
	return mutexes[((uintptr_t)atm / sizeof(*atm)) % MUTEXES_SIZE];
}

int atm_ptr_cmpxchg(struct atm_ptr *atm, void *oldvalue, void *newvalue)
{
	int r;
	mt_mutex_t *m = atm_ptr_get_mutex(atm);

	mt_mutex_lock(m);
	if (atm->x == oldvalue) {
		r = 1;
		atm->x = newvalue;
	} else {
		r = 0;
	}
	mt_mutex_unlock(m);

	return r;
}

#define NEED_ATM_PTR_STORE
#define NEED_ATM_PTR_LOAD

#endif

#ifdef NEED_ATM_LOAD
int32_t atm_load(struct atm *atm)
{
	int32_t val;
	do {
		val = atm->x;
	} while (!atm_cmpxchg(atm, val, val));
	return val;
}
#endif

#ifdef NEED_ATM_STORE
void atm_store(struct atm *atm, int32_t x)
{
	int32_t val;
	do {
		val = atm->x;
	} while (!atm_cmpxchg(atm, val, x));
}
#endif

#ifdef NEED_ATM_PTR_LOAD
void *atm_ptr_load(struct atm_ptr *atm)
{
	void *val;
	do {
		val = atm->x;
	} while (!atm_ptr_cmpxchg(atm, val, val));
	return val;
}
#endif

#ifdef NEED_ATM_PTR_STORE
void atm_ptr_store(struct atm_ptr *atm, void *x)
{
	void *val;
	do {
		val = atm->x;
	} while (!atm_ptr_cmpxchg(atm, val, x));
}
#endif
