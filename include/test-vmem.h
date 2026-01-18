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

#ifndef SCHISM_TEST_VMEM_H_
#define SCHISM_TEST_VMEM_H_

#include "headers.h"

/* basic memory protection.
 * This is not really guaranteed to make actual memory guidelines.
 * For example x86 machines imply VMEM_READ when VMEM_WRITE is set.
 * There is no exec ;) */

#define VMEM_READ 0x01
#define VMEM_WRITE 0x02

int vmem_protect(void *chunk, uint32_t flags);
void *vmem_alloc(size_t sz, uint32_t flags);
void vmem_free(void *chunk);
uintptr_t vmem_pagesize(void);

#endif /*SCHISM_TEST_VMEM_H_*/