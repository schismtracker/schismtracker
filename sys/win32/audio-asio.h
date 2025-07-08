/**
 * Incomplete ASIO interface definitions -- reverse-engineered from existing
 * binaries and various public sources.
 * 
 * Copyright (C) 2025 Paper
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
**/

#ifndef ASIO_INCL_H_
#define ASIO_INCL_H_

#include <stdint.h>
#include <windows.h>

typedef uint32_t AsioBool; /* always 0 or 1 */

/* ------------------------------------------------------------------------ */
/* Most* ASIO functions return error in a 32-bit signed integer.
 * Negative values notate an error.
 * Errors that I have actually encountered are defined below.
 * There are likely more that I don't know about. */

#define ASIO_ERROR_SUCCESS (0)
/* returned by FL Studio ASIO for values 3, 9, 10, and 12
 * I'm assuming it's success, because it's positive :) */
#define ASIO_ERROR_FUTURE_SUCCESS (1061701536)
/* this gets returned with an invalid future selector ? */
#define ASIO_ERROR_FUTURE_INVALID (-1000)
#define ASIO_ERROR_INVALID_PARAMETER (-998)

/* caused by calling some functions without properly calling Asio_Init() */
#define ASIO_ERROR_UNINITIALIZED (-997)

/* caused by calling CheckSampleRate() with a too low sample rate
 * (oddly enough, FlexASIO's SetSampleRate() doesn't return this
 * error, even when the sample rate is invalid. bug?) */
#define ASIO_ERROR_INVALID_RATE (-995)

/* returned by FL Studio ASIO if CreateBuffers() fails for whatever reason */
#define ASIO_ERROR_INTERNAL (-994)

typedef int32_t AsioError;

/* ------------------------------------------------------------------------ */

//#define ASIO_SAMPLE_TYPE_INT8LE (15) // ?

/* FlexASIO can do these four */
#define ASIO_SAMPLE_TYPE_INT16LE   (16)
#define ASIO_SAMPLE_TYPE_INT24LE   (17)
#define ASIO_SAMPLE_TYPE_INT32LE   (18)
#define ASIO_SAMPLE_TYPE_FLOAT32LE (19)
//#define ASIO_SAMPLE_TYPE_FLOAT64LE (20) // ?
typedef uint32_t AsioSampleType;

/* ------------------------------------------------------------------------ */

struct AsioClockSource {
	/* my ASIO drivers always fill this in with some random dummy data,
	 * and I have no idea what they're SUPPOSED to be */
	uint32_t unknown1; /* always 0x00000000 */
	uint32_t unknown2; /* always 0xFFFFFFFF */
	uint32_t unknown3; /* always 0xFFFFFFFF */
	uint32_t unknown4; /* always 0x00000001 */

	/* ASIO2WASAPI: strcpy_s(..., 0x20, "Internal clock"); */
	char name[32];
};

struct AsioBuffers {
	/* --- INPUT */
	AsioBool input;
	uint32_t channel;

	/* --- OUTPUT */
	void *ptrs[2];
};

struct AsioChannelInfo {
	/* --- INPUT */
	uint32_t index;
	AsioBool input;

	/* --- OUTPUT */
	AsioBool unknown1; /* Seems to always be 0 or 1 and is 32-bit */
	uint32_t unknown2; /* Seems to always be 0 ??? */
	AsioSampleType sample_type; /* One of the enum values #defined above */

	/* FlexASIO does strcpy_s with a cap of 0x20 into here */
	char name[32];
};

/* ------------------------------------------------------------------------ */
/* constants for buffer callback message handler.
 * classes range from 1..15; only ones I have actually figured out
 * are documented here. */

/* return 1 if the class is supported by the message handler */
#define ASIO_CLASS_SUPPORTS_CLASS (1)

/* return the ASIO interface version the client supports
 * this never passes any extra data (or none that i've seen anyway) */
#define ASIO_CLASS_ASIO_VERSION (2)

/* return 1 to get the driver to call buffer_flip_ex instead of
 * buffer_flip (added in a later version of the interface??) */
#define ASIO_CLASS_SUPPORTS_BUFFER_FLIP_EX (7)

struct AsioCreateBufferCallbacks {
	/* gets called any time the buffer is flipped
	 *
	 * buf: the buffer to fill
	 * unknown1: TODO */
	void (__cdecl *buffer_flip)(uint32_t buf, uint32_t unknown1);
	/* no idea what this one is; nothing I have calls it */
	void (__cdecl *unk2)(void);
	/* message handler:
	 * cls: class of message 'ASIO_CLASS_*'
	 * msg: the actual message itself
	 * unknown1: TODO
	 * unknown2: TODO */
	uint32_t (__cdecl *msg)(uint32_t cls, uint32_t msg, void *unknown1,
		void *unknown2);
	/* an expanded form of buffer_flip; takes in a pointer
	 * and returns one. I don't really know what it's for though. */
	void *(__cdecl *buffer_flip_ex)(void *unk1, uint32_t buf, uint32_t unk2);
};

/* ------------------------------------------------------------------------ */
/* now, the actual IAsio structure itself */

typedef struct IAsioVtbl IAsioVtbl;
typedef struct IAsio IAsio;

struct IAsioVtbl {

#define ASIO_FUNC(type, name, paramswtype, params, callconv) \
	type (callconv *name) paramswtype;

#include "audio-asio-vtable.h"

};

struct IAsio {
	CONST_VTBL IAsioVtbl *lpVtbl;
};

/* ------------------------------------------------------------------------ */
/* Optional C-style wrappers, in the spirit of COM #define OBJCMACROS */

#ifdef ASIO_C_WRAPPERS

#define ASIO_FUNC(type, name, paramswtype, params, callconv) \
	static inline type IAsio_##name paramswtype \
	{ \
		return This->lpVtbl->name params; \
	}

#include "audio-asio-vtable.h"

#endif /* ASIO_C_WRAPPERS */

#endif
