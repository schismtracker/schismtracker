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

#include "headers.h"

#include "test-vmem.h"

/* Fallback pagesize; assume 64KiB */
#define VMEM_FALLBACK_PAGESIZE (65536)

#if defined(HAVE_MMAP) && defined(HAVE_MPROTECT)
# include <sys/mman.h>
#endif

#if defined(HAVE_MMAP) && defined(HAVE_MPROTECT) && (defined(MAP_ANONYMOUS) || defined(MAP_ANON))
# define VMEM_MPROTECT
# if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#  define MAP_ANONYMOUS MAP_ANON
# endif
#elif defined(SCHISM_WIN32)
# define VMEM_WIN32
# include <windows.h>
#endif

#if defined(VMEM_MPROTECT) || defined(VMEM_WIN32)
/* This is way more than enough for now */
# define VMEM_TABLE_SIZE (4096)

struct vmem_table_entry {
	/* -- key */
	void *ptr;

	/* -- values */
	size_t sz;
};

static struct vmem_table_entry vmem_table[VMEM_TABLE_SIZE];

static struct vmem_table_entry *vmem_get_entry(void *x)
{
	size_t i;

	for (i = 0; i < VMEM_TABLE_SIZE; i++)
		if (vmem_table[i].ptr == x)
			return vmem_table + i;

	return NULL;
}

static void vmem_kill_entry(struct vmem_table_entry *x)
{
	x->ptr = NULL;
	x->sz  = 0; /* Eh */
}

static int vmem_add_entry(void *x, size_t sz)
{
	/* find an unused entry */
	struct vmem_table_entry *r = vmem_get_entry(NULL);

	if (!r)
		return -1;

	r->ptr = x;
	r->sz  = sz;

	return 0;
}
#endif

#ifdef VMEM_MPROTECT
static int vmem_convflags(uint32_t flags)
{
	int prot = 0;
	if (flags & VMEM_READ)
		prot |= PROT_READ;
	if (flags & VMEM_WRITE)
		prot |= PROT_WRITE;
	return prot;
}

int vmem_protect(void *chunk, uint32_t flags)
{
	struct vmem_table_entry *r;
    int x;

	r = vmem_get_entry(chunk);

	while ((x = mprotect(chunk, r->sz, vmem_convflags(flags))) != 0 && errno == EINTR);
    if (x != 0) {
    	perror("mprotect");
        return -1;
    }

	return 0;
}

void *vmem_alloc(size_t sz, uint32_t flags)
{
	struct vmem_table_entry *r;

    /* handle signals properly */
	while (!(r = mmap(NULL, sz, vmem_convflags(flags), MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) && errno == EINTR);
    if (!r)
        return NULL;

    if (vmem_add_entry(r, sz) < 0) {
    	while (munmap(r, sz) != 0 && errno == EINTR);
    	return NULL;
    }

	return r;
}

void vmem_free(void *chunk)
{
	struct vmem_table_entry *r;

	if (!chunk) return;

	r = vmem_get_entry(chunk);
	if (!r) return; /* ? */

	while (munmap(chunk, r->sz) != 0 && errno == EINTR);

	vmem_kill_entry(r);
}
#elif defined(VMEM_WIN32)
static DWORD vmem_convflags(uint32_t flags)
{
	return (flags & VMEM_WRITE) ? PAGE_READWRITE
		: (flags & VMEM_READ) ? PAGE_READONLY
		: PAGE_NOACCESS;
}

int vmem_protect(void *chunk, uint32_t flags)
{
	struct vmem_table_entry *r;
	DWORD xyzzy;

	if (!chunk) return -1; /* ok */

	r = vmem_get_entry(chunk);
	if (!r) return -1;

	if (!VirtualProtect(chunk, r->sz, vmem_convflags(flags), &xyzzy))
		return -1;

	return 0;
}

void *vmem_alloc(size_t sz, uint32_t flags)
{
	LPVOID x;
	struct vmem_table_entry *r;

	x = VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, vmem_convflags(flags));
	if (!x)
		return NULL;

	if (vmem_add_entry(x, sz) < 0) {
		VirtualFree(x, 0, MEM_RELEASE);
		return NULL;
	}

	return x;
}

void vmem_free(void *chunk)
{
	struct vmem_table_entry *r;

	r = vmem_get_entry(chunk);
	if (!r) return;

	VirtualFree(chunk, 0, MEM_RELEASE);

	vmem_kill_entry(r);
}
#else
int vmem_protect(void *chunk, uint32_t flags)
{
	/* fake it til you make it */
	return 0;
}

void *vmem_alloc(size_t sz, uint32_t flags)
{
	/* fallback to malloc() */
	return malloc(sz);
}

void vmem_free(void *chunk)
{
	free(chunk);
}
#endif

#if defined(HAVE_SYSCONF)
# include <unistd.h>

# if !defined(_SC_PAGESIZE) && defined(_SC_PAGE_SIZE)
#  define _SC_PAGESIZE _SC_PAGE_SIZE
# endif
#endif

uintptr_t vmem_pagesize(void)
{
#if defined(HAVE_SYSCONF)
	{
		long r;

		r = sysconf(_SC_PAGESIZE);
		if (r >= 1)
			return r;
	}
#endif

#if defined(VMEM_WIN32)
	{
		SYSTEM_INFO sys;
		GetSystemInfo(&sys);

		return sys.dwPageSize;
	}
#endif

	return VMEM_FALLBACK_PAGESIZE;
}
