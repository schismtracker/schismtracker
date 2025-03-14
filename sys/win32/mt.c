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

#define MSVC_EXCEPTION_NAME_CODE UINT32_C(0x406D1388)

// Multithreading implementation for Win32
//
// For the most part, this was created because of SDL 1.2 using the CreateMutex() API,
// even though critical sections existed within Windows since NT 3.1 (and possibly
// this can be attributed to MSDN for having wrong minimum system requirements)
// In short, Win32 mutexes are kernel-level, while critical sections are process-level.
// This means, theoretically, there should be no calling into the kernel when using
// any multithreading structures (including mutexes, which we use quite a lot in the
// source, especially for MIDI and events) making in particular MIDI quite a bit faster
// on older windows systems. Additionally we implement the SRWlock API that SDL uses
// as well (though I can't say for sure exactly how performant it is. mileage may vary)

/* ------------------------------------ */

static PVOID (WINAPI *KERNEL32_AddVectoredExceptionHandler)(ULONG, PVECTORED_EXCEPTION_HANDLER);
static ULONG (WINAPI *KERNEL32_RemoveVectoredExceptionHandler)(PVOID);
static BOOL (WINAPI *KERNEL32_IsDebuggerPresent)(void); // NT 4+

// not necessarily kernel32, but most likely. sometimes it can be kernelbase!
static HRESULT (WINAPI *KERNEL32_SetThreadDescription)(HANDLE, PCWSTR); // win10+

struct mt_thread {
	HANDLE thread;
	int status;

	char *name;

	schism_thread_function_t func;
	void *userdata;
};

static LONG __stdcall win32_exception_handler_noop(EXCEPTION_POINTERS *info)
{
	return (info && info->ExceptionRecord && info->ExceptionRecord->ExceptionCode == MSVC_EXCEPTION_NAME_CODE)
		? EXCEPTION_CONTINUE_EXECUTION
		: EXCEPTION_CONTINUE_SEARCH;
}

// This raises an exception that notifies the current debugger
// about a name change.
static inline void win32_raise_name_exception(const char *name)
{
	char *name_a = charset_iconv_easy(name, CHARSET_UTF8, CHARSET_ANSI);

	if (name_a) {
		// Based on other code that uses ugly non-standard packing; this is only
		// tested for x86 and x86_64, no idea about arm.

		SCHISM_STATIC_ASSERT(sizeof(ULONG_PTR) >= sizeof(DWORD), "This code assumes ULONG_PTR is at least the size of a DWORD");
		SCHISM_STATIC_ASSERT(sizeof(ULONG_PTR) % sizeof(DWORD) == 0, "This code assumes the size of ULONG_PTR is a multiple of the size of a DWORD");

#define DIVIDE_ROUNDING_UP(x, y) (((x) + (y) - 1) / (y))
		union {
			DWORD dw;
			ULONG_PTR ulp;
		} info[2 + DIVIDE_ROUNDING_UP(sizeof(DWORD) * 2, sizeof(ULONG_PTR))] = {0};
#undef DIVIDE_ROUNDING_UP

		info[0].dw = 0x1000; // Magic number
		info[1].ulp = (ULONG_PTR)name_a; // ANSI string w/ the name of the thread
		{
			// These two DWORDs are either packed into one or two ULONG_PTRs, depending
			// on whether it's 32 or 64-bit; either way, they're just right beside each
			// other in memory, so we can just do this.

			static const DWORD dw[2] = { -1 /* Thread ID (-1 for current thread) */, 0 /* Reserved */};
			memcpy(info + 2, dw, sizeof(dw));
		}

		RaiseException(MSVC_EXCEPTION_NAME_CODE, 0, ARRAY_SIZE(info), (const ULONG_PTR *)&info);

		free(name_a);
	}
}

static unsigned int __stdcall SCHISM_FORCE_ALIGN_ARG_POINTER win32_dummy_thread_func(void *userdata)
{
	mt_thread_t *thread = userdata;

	if (thread->name) {
		if (KERNEL32_SetThreadDescription) {
			LPWSTR strw = charset_iconv_easy(thread->name, CHARSET_UTF8, CHARSET_WCHAR_T);
			if (strw) {
				KERNEL32_SetThreadDescription(GetCurrentThread(), strw);
				free(strw);
			}
		}

		// SOMEBODY TOUCHA MY SPAGHET!
		if (KERNEL32_AddVectoredExceptionHandler && KERNEL32_RemoveVectoredExceptionHandler) {
			PVOID handler = KERNEL32_AddVectoredExceptionHandler(1, win32_exception_handler_noop);
			if (handler) {
				win32_raise_name_exception(thread->name);
				KERNEL32_RemoveVectoredExceptionHandler(handler);
			} // else ... ?
		} else if (KERNEL32_IsDebuggerPresent && KERNEL32_IsDebuggerPresent()) {
			win32_raise_name_exception(thread->name);
		}
	}

	thread->status = thread->func(thread->userdata);

	_endthreadex(0);

	return 0;
}

mt_thread_t *win32_thread_create(schism_thread_function_t func, const char *name, void *userdata)
{
	mt_thread_t *thread = mem_calloc(1, sizeof(*thread));

	thread->func = func;
	thread->userdata = userdata;
	thread->name = (name) ? str_dup(name) : NULL;

	unsigned int threadid;
	thread->thread = (HANDLE)_beginthreadex(NULL, 0, win32_dummy_thread_func, thread, 0, &threadid);
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

	free(thread->name);

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
static void (WINAPI *KERNEL32_InitializeCriticalSection)(LPCRITICAL_SECTION);
static DWORD (WINAPI *KERNEL32_SetCriticalSectionSpinCount)(LPCRITICAL_SECTION, DWORD);
static void (WINAPI *KERNEL32_DeleteCriticalSection)(LPCRITICAL_SECTION);
static void (WINAPI *KERNEL32_EnterCriticalSection)(LPCRITICAL_SECTION);
static void (WINAPI *KERNEL32_LeaveCriticalSection)(LPCRITICAL_SECTION);

static void (WINAPI *KERNEL32_InitializeSRWLock)(PSRWLOCK);
static void (WINAPI *KERNEL32_AcquireSRWLockExclusive)(PSRWLOCK);
static void (WINAPI *KERNEL32_ReleaseSRWLockExclusive)(PSRWLOCK);

// Should be set on init and never touched again until quit.
static enum {
	WIN32_MUTEX_IMPL_MUTEX = 0,
	WIN32_MUTEX_IMPL_CRITICALSECTION,
	WIN32_MUTEX_IMPL_SRWLOCK,
} win32_mutex_impl = WIN32_MUTEX_IMPL_MUTEX;

struct mt_mutex {
	union {
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
	mt_mutex_t *mutex = mem_calloc(1, sizeof(*mutex));

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
		// Setting a higher spin count generally results in better
		// performance on multi-core/multi-processor systems.
		if (KERNEL32_SetCriticalSectionSpinCount)
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

// Condition variables, emulated using semaphores on systems prior
// to Vista. Implementation using semaphores heavily borrowed from
// the BeOS condition variable emulation, by Christopher Tate and
// Owen Smith, including most comments.

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
	mt_cond_t *cond = mem_calloc(1, sizeof(*cond));

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

// this function is somewhat useless if we don't know whether we timed out or not
void win32_cond_wait_timeout(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t timeout)
{
	switch (win32_cond_impl) {
	case WIN32_COND_IMPL_FAKE:
		win32_cond_wait_fake_impl_(cond, mutex, &timeout);
		break;
	case WIN32_COND_IMPL_VISTA:
		win32_cond_wait_vista_impl_(cond, mutex, &timeout);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////////

static void *lib_kernel32 = NULL;
static void *lib_kernelbase = NULL;

static int win32_threads_init(void)
{
	lib_kernel32 = loadso_object_load("KERNEL32.DLL");
	lib_kernelbase = loadso_object_load("KERNELBASE.DLL");

	if (lib_kernel32) {
		KERNEL32_AddVectoredExceptionHandler = loadso_function_load(lib_kernel32, "AddVectoredExceptionHandler");
		KERNEL32_RemoveVectoredExceptionHandler = loadso_function_load(lib_kernel32, "RemoveVectoredExceptionHandler");
		KERNEL32_SetThreadDescription = loadso_function_load(lib_kernel32, "SetThreadDescription");
		KERNEL32_IsDebuggerPresent = loadso_function_load(lib_kernel32, "IsDebuggerPresent");

		KERNEL32_InitializeCriticalSection = loadso_function_load(lib_kernel32, "InitializeCriticalSection");
		KERNEL32_SetCriticalSectionSpinCount = loadso_function_load(lib_kernel32, "SetCriticalSectionSpinCount");
		KERNEL32_DeleteCriticalSection = loadso_function_load(lib_kernel32, "DeleteCriticalSection");
		KERNEL32_EnterCriticalSection = loadso_function_load(lib_kernel32, "EnterCriticalSection");
		KERNEL32_LeaveCriticalSection = loadso_function_load(lib_kernel32, "LeaveCriticalSection");

		KERNEL32_InitializeSRWLock = loadso_function_load(lib_kernel32, "InitializeSRWLock");
		KERNEL32_AcquireSRWLockExclusive = loadso_function_load(lib_kernel32, "AcquireSRWLockExclusive");
		KERNEL32_ReleaseSRWLockExclusive = loadso_function_load(lib_kernel32, "ReleaseSRWLockExclusive");

		KERNEL32_InitializeConditionVariable = loadso_function_load(lib_kernel32, "InitializeConditionVariable");
		KERNEL32_WakeConditionVariable = loadso_function_load(lib_kernel32, "WakeConditionVariable");
		KERNEL32_SleepConditionVariableSRW = loadso_function_load(lib_kernel32, "SleepConditionVariableSRW");
		KERNEL32_SleepConditionVariableCS = loadso_function_load(lib_kernel32, "SleepConditionVariableCS");
	} else {
		// reset all to null.
		KERNEL32_AddVectoredExceptionHandler = NULL;
		KERNEL32_RemoveVectoredExceptionHandler = NULL;
		KERNEL32_SetThreadDescription = NULL;
		KERNEL32_IsDebuggerPresent = NULL;

		KERNEL32_InitializeCriticalSection = NULL;
		KERNEL32_SetCriticalSectionSpinCount = NULL;
		KERNEL32_DeleteCriticalSection = NULL;
		KERNEL32_EnterCriticalSection = NULL;
		KERNEL32_LeaveCriticalSection = NULL;

		KERNEL32_InitializeSRWLock = NULL;
		KERNEL32_AcquireSRWLockExclusive = NULL;
		KERNEL32_ReleaseSRWLockExclusive = NULL;

		KERNEL32_InitializeConditionVariable = NULL;
		KERNEL32_WakeConditionVariable = NULL;
		KERNEL32_SleepConditionVariableSRW = NULL;
		KERNEL32_SleepConditionVariableCS = NULL;
	}

	if (lib_kernelbase && !KERNEL32_SetThreadDescription) {
		KERNEL32_SetThreadDescription = loadso_function_load(lib_kernelbase, "SetThreadDescription");
	}

	const int critsec_ok = KERNEL32_InitializeCriticalSection && KERNEL32_DeleteCriticalSection && KERNEL32_EnterCriticalSection && KERNEL32_LeaveCriticalSection;
	const int srwlock_ok = KERNEL32_InitializeSRWLock && KERNEL32_AcquireSRWLockExclusive && KERNEL32_ReleaseSRWLockExclusive;

	if (srwlock_ok) {
		win32_mutex_impl = WIN32_MUTEX_IMPL_SRWLOCK;
	} else if (critsec_ok) {
		win32_mutex_impl = WIN32_MUTEX_IMPL_CRITICALSECTION;
	} else {
		win32_mutex_impl = WIN32_MUTEX_IMPL_MUTEX;
	}

	if (KERNEL32_InitializeConditionVariable && KERNEL32_WakeConditionVariable) {
		win32_cond_impl = WIN32_COND_IMPL_VISTA;

		switch (win32_mutex_impl) {
		case WIN32_MUTEX_IMPL_SRWLOCK:
			if (KERNEL32_SleepConditionVariableSRW)
				break;
			SCHISM_FALLTHROUGH;
		case WIN32_MUTEX_IMPL_CRITICALSECTION:
			if (KERNEL32_SleepConditionVariableCS && critsec_ok) {
				// This might be srwlock, so set it to critical section to be safe
				win32_mutex_impl = WIN32_MUTEX_IMPL_CRITICALSECTION;
				break;
			}
			SCHISM_FALLTHROUGH;
		default:
			win32_cond_impl = WIN32_COND_IMPL_FAKE;
			break;
		}
	} else {
		win32_cond_impl = WIN32_COND_IMPL_FAKE;
	}

#if 0
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
#endif

	return 1;
}

static void win32_threads_quit(void)
{
	if (lib_kernel32) {
		loadso_object_unload(lib_kernel32);
		lib_kernel32 = NULL;
	}

	if (lib_kernelbase) {
		loadso_object_unload(lib_kernelbase);
		lib_kernelbase = NULL;
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
