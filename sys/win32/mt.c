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

#define _WIN32_WINNT 0x0600 // Windows Vista

#include "headers.h"
#include "mem.h"
#include "log.h"
#include "loadso.h"
#include "backend/mt.h"

#include <windows.h>
#include <assert.h>

// This file consists of a mostly-working-ish multithreading implementation
// for win32

/* ------------------------------------ */

struct mt_thread {
	HANDLE thread;
	int status;

	schism_thread_function_t func;
	void *userdata;
};

static unsigned int __stdcall SCHISM_FORCE_ALIGN_ARG_POINTER win32_dummy_thread_func(void *userdata)
{
	mt_thread_t *thread = userdata;

	thread->status = thread->func(thread->userdata);

	_endthreadex(0);

	return 0;
}

mt_thread_t *win32_thread_create(schism_thread_function_t func, const char *name, void *userdata)
{
	mt_thread_t *thread = mem_alloc(sizeof(*thread));

	thread->func = func;
	thread->userdata = userdata;

	unsigned int xyzzy;
	thread->thread = (HANDLE)_beginthreadex(NULL, 0, win32_dummy_thread_func, thread, 0, &xyzzy);
	if (!thread->thread) {
		free(thread);
		return NULL;
	}

	return thread;
}

void win32_thread_wait(mt_thread_t *thread, int *status)
{
	if (!thread) return;

	WaitForSingleObject(thread->thread, INFINITE);
	CloseHandle(thread->thread);

	if (status) *status = thread->status;

	free(thread);
}

void win32_thread_set_priority(int priority)
{
	// ok
	int npri;

	switch (priority) {
#define PRIORITY(x, y) case MT_THREAD_PRIORITY_##x: npri = THREAD_PRIORITY_##y; break 
	PRIORITY(LOW, LOWEST);
	PRIORITY(NORMAL, NORMAL);
	PRIORITY(HIGH, HIGHEST);
	PRIORITY(TIME_CRITICAL, TIME_CRITICAL);
	default: return;
#undef PRIORITY
	}

	SetThreadPriority(GetCurrentThread(), npri);
}

// returns the current thread's ID
static mt_thread_id_t win32_thread_id(void)
{
	return GetCurrentThreadId();
}

/* -------------------------------------------------------------- */
/* mutexes */

// Critical section pointers
static void (WINAPI *KERNEL32_InitializeCriticalSection)(LPCRITICAL_SECTION) = NULL;
static DWORD (WINAPI *KERNEL32_SetCriticalSectionSpinCount)(LPCRITICAL_SECTION, DWORD) = NULL;
static void (WINAPI *KERNEL32_DeleteCriticalSection)(LPCRITICAL_SECTION) = NULL;
static void (WINAPI *KERNEL32_EnterCriticalSection)(LPCRITICAL_SECTION) = NULL;
static void (WINAPI *KERNEL32_LeaveCriticalSection)(LPCRITICAL_SECTION) = NULL;

static void (WINAPI *KERNEL32_InitializeSRWLock)(PSRWLOCK) = NULL;
static void (WINAPI *KERNEL32_AcquireSRWLockExclusive)(PSRWLOCK) = NULL;
static void (WINAPI *KERNEL32_ReleaseSRWLockExclusive)(PSRWLOCK) = NULL;

// Should be set on init and never touched again until quit.
static enum {
	WIN32_MUTEX_IMPL_MUTEX = 0,
	WIN32_MUTEX_IMPL_CRITICALSECTION,
	WIN32_MUTEX_IMPL_SRWLOCK,
} win32_mutex_impl = WIN32_MUTEX_IMPL_MUTEX;

struct mt_mutex {
	/* union ? */struct {
		HANDLE mutex;

		CRITICAL_SECTION critsec;

		struct {
			SRWLOCK srw;

			uint32_t count;

			// thread ID of owner
			// zero is never a valid ID, as implied
			// via the docs for GetThreadId(), so
			// this is initialized to zero.
			DWORD owner;
		} srwlock;
	} impl;
};

mt_mutex_t *win32_mutex_create(void)
{
	mt_mutex_t *mutex = mem_alloc(sizeof(*mutex));

	switch (win32_mutex_impl) {
	case WIN32_MUTEX_IMPL_MUTEX:
		mutex->impl.mutex = CreateMutexA(NULL, FALSE, NULL);
		if (!mutex->impl.mutex) {
			free(mutex);
			return NULL;
		}
		break;
	case WIN32_MUTEX_IMPL_CRITICALSECTION:
		KERNEL32_InitializeCriticalSection(&mutex->impl.critsec);
		KERNEL32_SetCriticalSectionSpinCount(&mutex->impl.critsec, 2000u);
		break;
	case WIN32_MUTEX_IMPL_SRWLOCK:
		KERNEL32_InitializeSRWLock(&mutex->impl.srwlock.srw);
		mutex->impl.srwlock.count = 0;
		mutex->impl.srwlock.owner = 0; // never a valid ID
		break;
	}


	return mutex;
}

void win32_mutex_delete(mt_mutex_t *mutex)
{
	if (!mutex) return;

	switch (win32_mutex_impl) {
	case WIN32_MUTEX_IMPL_MUTEX:
		CloseHandle(mutex->impl.mutex);
		break;
	case WIN32_MUTEX_IMPL_CRITICALSECTION:
		KERNEL32_DeleteCriticalSection(&mutex->impl.critsec);
		break;
	case WIN32_MUTEX_IMPL_SRWLOCK:
		// STOOPID! YOU'RE SO STOOPID!
		break;
	}
	free(mutex);
}

void win32_mutex_lock(mt_mutex_t *mutex)
{
	if (!mutex) return;

	switch (win32_mutex_impl) {
	case WIN32_MUTEX_IMPL_MUTEX:
		assert(WaitForSingleObject(mutex->impl.mutex, INFINITE) == WAIT_OBJECT_0);
		break;
	case WIN32_MUTEX_IMPL_CRITICALSECTION:
		KERNEL32_EnterCriticalSection(&mutex->impl.critsec);
		break;
	case WIN32_MUTEX_IMPL_SRWLOCK: {
		const DWORD id = GetCurrentThreadId();
		if (mutex->impl.srwlock.owner == id) {
			++mutex->impl.srwlock.count;
		} else {
			KERNEL32_AcquireSRWLockExclusive(&mutex->impl.srwlock.srw);
			assert(!mutex->impl.srwlock.count && !mutex->impl.srwlock.owner);
			mutex->impl.srwlock.count = 1;
			mutex->impl.srwlock.owner = id;
		}
		break;
	}
	}
}

void win32_mutex_unlock(mt_mutex_t *mutex)
{
	if (!mutex) return;

	switch (win32_mutex_impl) {
	case WIN32_MUTEX_IMPL_MUTEX:
		assert(ReleaseMutex(mutex->impl.mutex));
		break;
	case WIN32_MUTEX_IMPL_CRITICALSECTION:
		KERNEL32_LeaveCriticalSection(&mutex->impl.critsec);
		break;
	case WIN32_MUTEX_IMPL_SRWLOCK:
		if (mutex->impl.srwlock.owner == GetCurrentThreadId()) {
			if (!--mutex->impl.srwlock.count) {
				mutex->impl.srwlock.owner = 0;
				KERNEL32_ReleaseSRWLockExclusive(&mutex->impl.srwlock.srw);
			}
		} else {
			assert(!"You are in a maze of twisty little passages, all alike.");
		}
		break;
	}
}

/* -------------------------------------------------------------- */

static void (WINAPI *KERNEL32_InitializeConditionVariable)(PCONDITION_VARIABLE);
static BOOL (WINAPI *KERNEL32_SleepConditionVariableCS)(PCONDITION_VARIABLE, PCRITICAL_SECTION, DWORD);
static BOOL (WINAPI *KERNEL32_SleepConditionVariableSRW)(PCONDITION_VARIABLE, PSRWLOCK, DWORD, ULONG);
static void (WINAPI *KERNEL32_WakeConditionVariable)(PCONDITION_VARIABLE);

static enum {
	WIN32_COND_IMPL_FAKE = 0,
	WIN32_COND_IMPL_VISTA, // CONDITION_VARIABLE API
} win32_cond_impl = WIN32_COND_IMPL_FAKE;

struct mt_cond {
	union {
		struct {
			HANDLE sem;
			HANDLE handshake_sem;
			HANDLE signal_sem;
			int32_t nw; // number waiting
			int32_t ns; // number signaled
		} fake;

		CONDITION_VARIABLE vista;
	} impl;
};

void win32_cond_delete(mt_cond_t *cond);

mt_cond_t *win32_cond_create(void)
{
	mt_cond_t *cond = mem_alloc(sizeof(*cond));

	switch (win32_cond_impl) {
	case WIN32_COND_IMPL_FAKE:
		cond->impl.fake.sem = CreateSemaphoreA(NULL, 0, LONG_MAX, NULL);
		cond->impl.fake.handshake_sem = CreateSemaphoreA(NULL, 0, LONG_MAX, NULL);
		cond->impl.fake.signal_sem = CreateSemaphoreA(NULL, 1, LONG_MAX, NULL);
		cond->impl.fake.ns = 0;
		cond->impl.fake.nw = 0;

		if (!cond->impl.fake.sem || !cond->impl.fake.handshake_sem || !cond->impl.fake.signal_sem) {
			win32_cond_delete(cond);
			return NULL;
		}
		break;
	case WIN32_COND_IMPL_VISTA:
		KERNEL32_InitializeConditionVariable(&cond->impl.vista);
		break;
	}

	return cond;
}

void win32_cond_delete(mt_cond_t *cond)
{
	if (!cond) return;

	switch (win32_cond_impl) {
	case WIN32_COND_IMPL_FAKE:
		if (cond->impl.fake.sem) CloseHandle(cond->impl.fake.sem);
		if (cond->impl.fake.handshake_sem) CloseHandle(cond->impl.fake.handshake_sem);
		if (cond->impl.fake.signal_sem) CloseHandle(cond->impl.fake.signal_sem);
		break;
	case WIN32_COND_IMPL_VISTA:
		// LET'S SEE WHAT'S IN THE BOX!
		break;
	}

	free(cond);
}

void win32_cond_signal(mt_cond_t *cond)
{
	if (!cond) return;

	switch (win32_cond_impl) {
	case WIN32_COND_IMPL_FAKE:
		// we need exclusive access to the waiter count while we figure out whether
		// we need a handshake with an awakening waiter thread
		WaitForSingleObject(cond->impl.fake.signal_sem, INFINITE);

		// are there waiters to be awakened?
		if (cond->impl.fake.nw > cond->impl.fake.ns) {
			// inform the next awakening waiter that we need a handshake, then release
			// all the locks and block until we get the handshake.  We need to go through the
			// handshake process even if we're interrupted, to avoid breaking the CV, so we
			// just set the eventual return code if we are interrupted in the middle.
			cond->impl.fake.ns += 1;
			ReleaseSemaphore(cond->impl.fake.sem, 1, NULL);
			ReleaseSemaphore(cond->impl.fake.signal_sem, 1, NULL);
			WaitForSingleObject(cond->impl.fake.handshake_sem, INFINITE);
		} else {
			// nobody is waiting, so the signal operation is a no-op
			ReleaseSemaphore(cond->impl.fake.signal_sem, 1, NULL);
		}
		break;
	case WIN32_COND_IMPL_VISTA:
		KERNEL32_WakeConditionVariable(&cond->impl.vista);
		break;
	}
}

static inline SCHISM_ALWAYS_INLINE int win32_cond_wait_fake_impl_(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t *timeout)
{
	int result = 0;

	// validate the arguments
	if (!cond || !mutex) return 0;

	// record the fact that we're waiting on the semaphore.  This action is
	// protected by a mutex because exclusive access to the waiter count is
	// needed by both waiting threads and signalling threads.  If someone interrupts
	// us while we're waiting for the lock (e.g. by calling kill() or send_signal()), we
	// abort and return the appropriate failure code.
	WaitForSingleObject(cond->impl.fake.signal_sem, INFINITE);
	cond->impl.fake.nw++;
	ReleaseSemaphore(cond->impl.fake.signal_sem, 1, NULL);

	// actually wait for a signal -- we have to unlock the mutex before calling the
	// underlying blocking primitive.  The potential preemption between unlocking
	// the mutex and calling acquire_sem() is why we needed to record, prior to
	// this point, that we're in the process of waiting on the condition variable.
	win32_mutex_unlock(mutex);

	result = (WaitForSingleObject(cond->impl.fake.sem, timeout ? *timeout : INFINITE) == WAIT_OBJECT_0);

	// we just awoke, either via a signal or by being interrupted.  If there's
	// a signaller running, he'll think he needs to handshake whether or not
	// we actually awoke due to his signal.  So, we reacquire the signalSem
	// mutex, and handshake if there's a positive signaller count.  It's critical
	// that we continue with the handshake process even if we've been interrupted,
	// so we just set the eventual error code and proceed with the CV state
	// unwinding in that case.
	WaitForSingleObject(cond->impl.fake.signal_sem, INFINITE);
	if (cond->impl.fake.ns > 0) {
		ReleaseSemaphore(cond->impl.fake.handshake_sem, 1, NULL);
		cond->impl.fake.ns--;
	}
	cond->impl.fake.nw--;
	ReleaseSemaphore(cond->impl.fake.signal_sem, 1, NULL);

	// always reacquire the mutex before returning, even in error cases
	win32_mutex_lock(mutex);

	return result;
}

static inline SCHISM_ALWAYS_INLINE int win32_cond_wait_vista_impl_(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t *timeout)
{
	switch (win32_mutex_impl) {
	case WIN32_MUTEX_IMPL_SRWLOCK: {
		const DWORD id = GetCurrentThreadId();

		if (mutex->impl.srwlock.count != 1 || mutex->impl.srwlock.owner != id)
			return 0;

		mutex->impl.srwlock.count = 0;
		mutex->impl.srwlock.owner = 0;

		int result = KERNEL32_SleepConditionVariableSRW(&cond->impl.vista, &mutex->impl.srwlock.srw, timeout ? *timeout : INFINITE, 0);

		// Mutex is always owned by us now
		assert(!mutex->impl.srwlock.count && !mutex->impl.srwlock.owner);
		mutex->impl.srwlock.count = 1;
		mutex->impl.srwlock.owner = id;

		return result;
	}
	case WIN32_MUTEX_IMPL_CRITICALSECTION:
		return KERNEL32_SleepConditionVariableCS(&cond->impl.vista, &mutex->impl.critsec, timeout ? *timeout : INFINITE);
	default:
		// Should never happen
		return 0;
	}
}

void win32_cond_wait(mt_cond_t *cond, mt_mutex_t *mutex)
{
	switch (win32_cond_impl) {
	case WIN32_COND_IMPL_FAKE:
		win32_cond_wait_fake_impl_(cond, mutex, NULL);
		break;
	case WIN32_COND_IMPL_VISTA:
		win32_cond_wait_vista_impl_(cond, mutex, NULL);
		break;
	}
}

// this function is useless if we don't know whether we timed out or not
void win32_cond_wait_timeout(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t timeout)
{
	switch (win32_cond_impl) {
	case WIN32_COND_IMPL_FAKE:
		win32_cond_wait_fake_impl_(cond, mutex, &timeout);
		break;
	case WIN32_COND_IMPL_VISTA:
		win32_cond_wait_vista_impl_(cond, mutex, NULL);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////////

void *lib_kernel32 = NULL;

static int win32_threads_init(void)
{
	lib_kernel32 = loadso_object_load("KERNEL32.DLL");

	KERNEL32_InitializeCriticalSection = loadso_function_load(lib_kernel32, "InitializeCriticalSection");
	KERNEL32_SetCriticalSectionSpinCount = loadso_function_load(lib_kernel32, "SetCriticalSectionSpinCount");
	KERNEL32_DeleteCriticalSection = loadso_function_load(lib_kernel32, "DeleteCriticalSection");
	KERNEL32_EnterCriticalSection = loadso_function_load(lib_kernel32, "EnterCriticalSection");
	KERNEL32_LeaveCriticalSection = loadso_function_load(lib_kernel32, "LeaveCriticalSection");

	KERNEL32_InitializeSRWLock = loadso_function_load(lib_kernel32, "InitializeSRWLock");
	KERNEL32_AcquireSRWLockExclusive = loadso_function_load(lib_kernel32, "AcquireSRWLockExclusive");
	KERNEL32_ReleaseSRWLockExclusive = loadso_function_load(lib_kernel32, "ReleaseSRWLockExclusive");

	const int critsec_ok = KERNEL32_InitializeCriticalSection && KERNEL32_SetCriticalSectionSpinCount && KERNEL32_DeleteCriticalSection && KERNEL32_EnterCriticalSection && KERNEL32_LeaveCriticalSection;
	const int srwlock_ok = KERNEL32_InitializeSRWLock && KERNEL32_AcquireSRWLockExclusive && KERNEL32_ReleaseSRWLockExclusive;

	if (srwlock_ok) {
		win32_mutex_impl = WIN32_MUTEX_IMPL_SRWLOCK;
	} else if (critsec_ok) {
		win32_mutex_impl = WIN32_MUTEX_IMPL_CRITICALSECTION;
	} else {
		win32_mutex_impl = WIN32_MUTEX_IMPL_MUTEX;
	}

	KERNEL32_InitializeConditionVariable = loadso_function_load(lib_kernel32, "InitializeConditionVariable");
	KERNEL32_WakeConditionVariable = loadso_function_load(lib_kernel32, "WakeConditionVariable");
	KERNEL32_SleepConditionVariableSRW = loadso_function_load(lib_kernel32, "SleepConditionVariableSRW");
	KERNEL32_SleepConditionVariableCS = loadso_function_load(lib_kernel32, "SleepConditionVariableCS");

	if (KERNEL32_InitializeConditionVariable && KERNEL32_WakeConditionVariable) {
		win32_cond_impl = WIN32_COND_IMPL_VISTA;

		switch (win32_mutex_impl) {
		case WIN32_MUTEX_IMPL_SRWLOCK:
			if (KERNEL32_SleepConditionVariableSRW)
				break;
			// fallthrough
		case WIN32_MUTEX_IMPL_CRITICALSECTION:
			if (KERNEL32_SleepConditionVariableCS && critsec_ok) {
				// This might be srwlock, so set it to critical section to be safe
				win32_mutex_impl = WIN32_MUTEX_IMPL_CRITICALSECTION;
				break;
			}
			// fallthrough
		default:
			win32_cond_impl = WIN32_COND_IMPL_FAKE;
			break;
		}
	} else {
		win32_cond_impl = WIN32_COND_IMPL_FAKE;
	}

	static const char *mutex_impl_names[] = {
		[WIN32_MUTEX_IMPL_MUTEX] = "Mutex",
		[WIN32_MUTEX_IMPL_CRITICALSECTION] = "CriticalSection",
		[WIN32_MUTEX_IMPL_SRWLOCK] = "SRWLock",
	};

	static const char *cond_impl_names[] = {
		[WIN32_COND_IMPL_FAKE] = "emulated",
		[WIN32_COND_IMPL_VISTA] = "Vista",
	};

	log_appendf(1, "WIN32: using %s mutexes and %s condition variables", mutex_impl_names[win32_mutex_impl], cond_impl_names[win32_cond_impl]);

	return 1;
}

static void win32_threads_quit(void)
{
	if (lib_kernel32) {
		loadso_object_unload(lib_kernel32);
		lib_kernel32 = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////

const schism_mt_backend_t schism_mt_backend_win32 = {
	.init = win32_threads_init,
	.quit = win32_threads_quit,

	.thread_create = win32_thread_create,
	.thread_wait = win32_thread_wait,
	.thread_set_priority = win32_thread_set_priority,
	.thread_id = win32_thread_id,

	.mutex_create = win32_mutex_create,
	.mutex_delete = win32_mutex_delete,
	.mutex_lock = win32_mutex_lock,
	.mutex_unlock = win32_mutex_unlock,

	.cond_create = win32_cond_create,
	.cond_delete = win32_cond_delete,
	.cond_signal = win32_cond_signal,
	.cond_wait = win32_cond_wait,
	.cond_wait_timeout = win32_cond_wait_timeout,
};
