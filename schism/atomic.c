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

/* FIXME this is way too primitive to do much of anything actually useful.
 * The most important thing we're missing is compare-and-swap. Virtually
 * any operation can be implemented with just that.
 *
 * ALSO: We should implement everything as explicitly 32-bit or 64-bit.
 * Then, the "pointer" version can simply forward to the 32-bit or 64-bit
 * versions depending on the architecture.
 *
 * We can do this by first implementing the bare functions taking volatile
 * pointers, and changing the definition of the pointer structure to have
 * a volatile union of pointer and int[32/64]_t. Then we can implement
 * everything else simply as calls to that function. :) */

#if defined(SCHISM_WIIU)
/* There is a critical bug in the WiiU processor, where atomics
 * do not work correctly. This is fixed on the OS side. */
# include <coreinit/atomic.h>
# include <coreinit/atomic64.h>
# define COREATM(NAME, TYPE) \
	TYPE atm##NAME##_load(struct atm##NAME *atm) \
	{ \
		return OSOrAtomic##NAME(&atm->x, 0); \
	} \
	\
	void atm##NAME##_store(struct atm##NAME *atm, TYPE x) \
	{ \
		OSSwapAtomic##NAME(&atm->x, x); \
	} \
	TYPE atm##NAME##_add(struct atm##NAME *atm, TYPE x) \
	{ \
		return OSAddAtomic##NAME(&atm->x, x); \
	}
# ifndef ATM_DEFINED
COREATM(/* none */, int32_t)
#  define ATM_DEFINED
# endif
# ifndef ATM64_DEFINED
COREATM(64, int64_t)
#  define ATM64_DEFINED
# endif
# undef COREATM
#endif

/* Retro68 has the functions declared, but they are not actually exported
 * anywhere, which causes a link error. */
#if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__) && !defined(SCHISM_MACOS)
# include <stdatomic.h>
# define C11ATM(NAME, TYPE) \
	TYPE atm##NAME##_load(struct atm##NAME *atm) \
	{ \
		return atomic_load((const _Atomic volatile TYPE *)&atm->x); \
	} \
	\
	void atm##NAME##_store(struct atm##NAME *atm, TYPE x) \
	{ \
		atomic_store((_Atomic volatile TYPE *)&atm->x, x); \
	} \
	TYPE atm##NAME##_add(struct atm##NAME *atm, TYPE x) \
	{ \
		return atomic_fetch_add((_Atomic volatile TYPE *)&atm->x, x); \
	}
# ifndef ATM_DEFINED
C11ATM(/* none */, int32_t)
#  define ATM_DEFINED
# endif
# ifndef ATM64_DEFINED
C11ATM(64, int64_t)
#  define ATM64_DEFINED
# endif
# undef C11ATM
#endif

#if SCHISM_GNUC_HAS_BUILTIN(__atomic_load, 4, 7, 0) && !defined(SCHISM_MACOS)
# define GNUCATM(NAME, TYPE) \
	TYPE atm##NAME##_load(struct atm##NAME *atm) \
	{ \
		TYPE r; \
		__atomic_load(&atm->x, &r, __ATOMIC_SEQ_CST); \
		return r; \
	} \
	\
	void atm##NAME##_store(struct atm##NAME *atm, TYPE x) \
	{ \
		__atomic_store(&atm->x, &x, __ATOMIC_SEQ_CST); \
	} \
	TYPE atm##NAME##_add(struct atm##NAME *atm, TYPE x) \
	{ \
		return __atomic_fetch_add(&atm->x, x, __ATOMIC_SEQ_CST); \
	}
# ifndef ATM_DEFINED
GNUCATM(/* none */, int32_t)
#  define ATM_DEFINED
# endif
# ifndef ATM64_DEFINED
GNUCATM(64, int64_t)
#  define ATM64_DEFINED
# endif
# undef GNUCATM
#endif

#if SCHISM_GNUC_HAS_BUILTIN(__sync_synchronize, 4, 1, 0)
/* I hope this is right */

#define GNUCATM(NAME, TYPE) \
	TYPE atm##NAME##_load(struct atm##NAME *atm) \
	{ \
		__sync_synchronize(); \
		return atm->x; \
	} \
	\
	void atm##NAME##_store(struct atm##NAME *atm, TYPE x) \
	{ \
		atm->x = x; \
		__sync_synchronize(); \
	} \
	TYPE atm##NAME##_add(struct atm##NAME *atm, TYPE x) \
	{ \
		return __sync_fetch_and_add(&atm->x, x); \
	}

# ifndef ATM_DEFINED
GNUCATM(/* none */, int32_t)
#  define ATM_DEFINED
# endif
# if !defined(ATM64_DEFINED) && !defined(__powerpc__)
GNUCATM(64, int64_t)
#  define ATM64_DEFINED
# endif
# undef GNUCATM
#endif

#if defined(SCHISM_WIN32)
/* Interlocked* */
# include <windows.h>

SCHISM_STATIC_ASSERT(sizeof(LONG) == sizeof(int32_t), "LONG must be 32-bit");
SCHISM_STATIC_ASSERT(sizeof(LONG64) == sizeof(int64_t), "LONGLONG must be 64-bit");

#if !defined(ATM_DEFINED)
int32_t atm_load(struct atm *atm)
{
	return InterlockedOr((volatile LONG *)&atm->x, 0);
}

void atm_store(struct atm *atm, int32_t x)
{
	InterlockedExchange((volatile LONG *)&atm->x, x);
}

int64_t atm_add(struct atm *atm, int32_t x)
{
	return InterlockedExchangeAdd((volatile LONG *)&atm->x, x);
}
# define ATM_DEFINED
#endif

#if !defined(ATM64_DEFINED)
int64_t atm64_load(struct atm64 *atm)
{
	return InterlockedOr64((volatile LONG64 *)&atm->x, 0);
}

void atm64_store(struct atm64 *atm, int64_t x)
{
	InterlockedExchange64((volatile LONG64 *)&atm->x, x);
}

int64_t atm64_add(struct atm64 *atm, int64_t x)
{
	return InterlockedExchangeAdd64((volatile LONG64 *)&atm->x, x);
}
# define ATM64_DEFINED
#endif

#endif

#if defined(__WATCOMC__) && defined(__386__)
SCHISM_STATIC_ASSERT(sizeof(void *) == sizeof(int32_t),
	"atomic code assumes that pointer is 32-bit");

static int32_t _watcom_xchg(volatile int32_t *a, int32_t v);
#pragma aux _watcom_xchg = \
	"lock xchg [ecx], eax" \
	parm [ecx] [eax] \
	value [eax] \
	modify exact [eax];

static int32_t _watcom_xadd(volatile int32_t *a, int32_t v);
#pragma aux _watcom_xadd = \
	"lock xadd [ecx], eax" \
	parm [ecx] [eax] \
	value [eax] \
	modify exact [eax];

#ifndef ATM_DEFINED
int32_t atm_load(struct atm *atm)
{
	return _watcom_xadd(&atm->x, 0);
}

void atm_store(struct atm *atm, int32_t x)
{
	_watcom_xchg(&atm->x, x);
}

int32_t atm_add(struct atm *atm, int32_t x)
{
	return _watcom_xadd(&atm->x, x);
}
# define ATM_DEFINED
#endif

#endif

#if !defined(USE_THREADS)

#ifndef ATM_DEFINED
int32_t atm_load(struct atm *atm)
{
	return atm->x;
}

void atm_store(struct atm *atm, int32_t x)
{
	atm->x = x;
}

int32_t atm_add(struct atm *atm, int32_t x)
{
	return (atm->x += x);
}
#define ATM_DEFINED
#endif

#ifndef ATM64_DEFINED
int64_t atm64_load(struct atm64 *atm)
{
	return atm->x;
}

void atm64_store(struct atm64 *atm, int64_t x)
{
	atm->x = x;
}

int64_t atm64_add(struct atm64 *atm, int64_t x)
{
	return (atm->x += x);
}
#define ATM64_DEFINED
#endif

#endif

/* TODO: SDL has atomics, probably with more platforms than
 * we support now. We should be able to import it. */

#include "mt.h"

#define MUTEXES_SIZE (16)

#if !defined(ATM_DEFINED) || !defined(ATM64_DEFINED) || ((SIZEOF_VOID_P != 4) && (SIZEOF_VOID_P != 8))
# define ATM_NEED_MUTEXES 1
#endif

#ifdef ATM_NEED_MUTEXES
static mt_mutex_t *mutexes[MUTEXES_SIZE] = {0};
#endif

int atm_init(void)
{
#ifdef ATM_NEED_MUTEXES
	{
		uint32_t i;

		for (i = 0; i < MUTEXES_SIZE; i++) {
			mutexes[i] = mt_mutex_create();
			if (!mutexes[i])
				return -1;
		}
	}
#endif

	/* at this point, the mutexes array should NEVER be touched again
	 * until we quit. */

	return 0;
}

void atm_quit(void)
{
#ifdef ATM_NEED_MUTEXES
	uint32_t i;

	for (i = 0; i < MUTEXES_SIZE; i++) {
		if (mutexes[i]) {
			mt_mutex_delete(mutexes[i]);
			mutexes[i] = NULL;
		}
	}
#endif
}

/* ------------------------------------------------------------------------ */

#ifdef ATM_NEED_MUTEXES
static inline SCHISM_ALWAYS_INLINE
mt_mutex_t *atm_get_mutex_impl(void *x, size_t align)
{
	return mutexes[((uintptr_t)x / align) % MUTEXES_SIZE];
}

#define atm_get_mutex(type, x) atm_get_mutex_impl(x, SCHISM_ALIGNOF(type))

#define ATM_IMPL(NAME, TYPE) \
	TYPE atm##NAME##_load(struct atm##NAME *atm) \
	{ \
		TYPE r; \
		mt_mutex_t *m = atm_get_mutex(struct atm##NAME, atm); \
	\
		mt_mutex_lock(m); \
		r = atm->x; \
		mt_mutex_unlock(m); \
	\
		return r; \
	} \
	\
	void atm##NAME##_store(struct atm##NAME *atm, TYPE x) \
	{ \
		mt_mutex_t *m = atm_get_mutex(struct atm##NAME, atm); \
	\
		mt_mutex_lock(m); \
		atm->x = x; \
		mt_mutex_unlock(m); \
	} \
	\
	TYPE atm##NAME##_add(struct atm##NAME *atm, TYPE x) \
	{ \
		TYPE r; \
		mt_mutex_t *m = atm_get_mutex(struct atm##NAME, atm); \
	\
		mt_mutex_lock(m); \
		r = (atm->x += x); \
		mt_mutex_unlock(m); \
	\
		return r; \
	}

#endif

#if !defined(ATM_DEFINED)
ATM_IMPL(/* none */, int32_t)
#endif
#if !defined(ATM64_DEFINED)
ATM_IMPL(64, int64_t)
#endif

int32_t atm_sub(struct atm *atm, int32_t x) { return atm_add(atm, -x); }
int64_t atm64_sub(struct atm64 *atm, int64_t x) { return atm64_add(atm, -x); }

/* pointer ---- */

#define ATM_PTR_IMPL(NAME, TYPE) \
	void *atm_ptr_load(struct atm_ptr *atm) \
	{ \
		return (void *)atm##NAME##_load(&atm->x); \
	} \
	\
	void atm_ptr_store(struct atm_ptr *atm, void *x) \
	{ \
		atm##NAME##_store(&atm->x, (TYPE)x); \
	}

#if SIZEOF_VOID_P == 8
ATM_PTR_IMPL(64, int64_t)
#elif SIZEOF_VOID_P == 4
ATM_PTR_IMPL(/* none */, int32_t)
#else
ATM_IMPL(_ptr, void *)
#endif
