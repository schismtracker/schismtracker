/**
 * Incomplete ASIO interface definitions -- reverse-engineered from existing
 * binaries and various public sources.
 * 
 * Copyright (C) 2025-2026 Paper
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

/* I don't care about C89 */
#include <stdint.h>

#ifdef _WIN32
# define ASIO_WIN32
#elif defined(macintosh) || defined(Macintosh) || defined(__MACOS__)
# define ASIO_MAC
#else
# error unsupported platform
#endif

/* __cdecl is only supported on win32 */
#ifdef ASIO_WIN32
# define ASIO_CDECL __cdecl
#else
# define ASIO_CDECL
#endif

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

/* USB ASIO driver on Mac OS 9 */
#define ASIO_SAMPLE_TYPE_INT16BE   (0)
#define ASIO_SAMPLE_TYPE_INT24BE   (1)
#define ASIO_SAMPLE_TYPE_INT32BE   (2)

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
	void (ASIO_CDECL *buffer_flip)(uint32_t buf, uint32_t unknown1);
	/* no idea what this one is; nothing I have calls it */
	void (ASIO_CDECL *unk2)(void);
	/* message handler:
	 * cls: class of message 'ASIO_CLASS_*'
	 * msg: the actual message itself
	 * unknown1: TODO
	 * unknown2: TODO */
	uint32_t (ASIO_CDECL *msg)(uint32_t cls, uint32_t msg, void *unknown1,
		void *unknown2);
	/* an expanded form of buffer_flip; takes in a pointer
	 * and returns one. I don't really know what it's for though. */
	void *(ASIO_CDECL *buffer_flip_ex)(void *unk1, uint32_t buf, uint32_t unk2);
};

/* ------------------------------------------------------------------------ */
/* IAsio functions */

typedef struct IAsio IAsio;

/* Initializes the IAsio structure.
 * Assume any and all function calls will fail if you DON'T call this.
 * This includes stuff like IAsio_GetDriverName, since that function
 * is "faked" under Macintosh. */
AsioError IAsio_Init(IAsio *This);
/* De-initializes the IAsio structure.
 * This functions also completely kills off the pointer, i.e. you should
 * not try to use the IAsio pointer after this. */
void IAsio_Quit(IAsio *This);
/* Get the driver name. */
void IAsio_GetDriverName(IAsio *This, char name[32]);
/* Get the driver version. */
uint32_t IAsio_GetDriverVersion(IAsio *This);
/* Get the last error message. */
void IAsio_GetErrorMessage(IAsio *This, char msg[128]);
/* Start/unpause playback */
AsioError IAsio_Start(IAsio *This);
/* Stop/pause playback */
AsioError IAsio_Stop(IAsio *This);
/* Gets the number of channels for input and output respectively. */
AsioError IAsio_GetChannels(IAsio *This, uint32_t *pinchns, uint32_t *poutchns);
/* Gets the latency in samples for input and output respectively. */
AsioError IAsio_GetLatencies(IAsio *This, uint32_t *pinlatency, uint32_t *poutlatency);
/* Gets the supported buffer size values.
 *  -- pmin: minimum supported buffer size.
 *  -- pmax: maximum supported buffer size.
 *  -- pwanted: the buffer size the driver wants you to use
 *  -- punknown: ??? */
AsioError IAsio_GetBufferSize(IAsio *This, uint32_t *pmin, uint32_t *pmax, uint32_t *pwanted, uint32_t *punknown);
/* Check whether a driver supports a given sample rate.
 *  -- rate: sample rate as a 64-bit IEEE floating-point number */
AsioError IAsio_SupportsSampleRate(IAsio *This, double rate);
/* Get the current sample rate the driver is using.
 *  -- prate: pointer to a 64-bit IEEE floating-point number receiving the sample rate */
AsioError IAsio_GetSampleRate(IAsio *This, double *prate);
/* Set the current sample rate the driver is using.
 *  -- rate: sample rate as a 64-bit IEEE floating-point number */
AsioError IAsio_SetSampleRate(IAsio *This, double rate);
/* Gets the supplied clock sources.
 * I'm not really sure what the point of this is, but hey, it's here.
 * None of these fields are documented at all, since I really can't be bothered.
 *  -- srcs: pointer to array of Asio clock sources
 *  -- size: on input, the size of the array pointed to by 'srcs'
 *           on output, the actual number of items returned */
AsioError IAsio_GetClockSources(IAsio *This, struct AsioClockSource *srcs, uint32_t *size);
/* Sets the current clock source.
 *  -- src: ??? */
AsioError IAsio_SetClockSource(IAsio *This, uint32_t src);
/* Gets the current sample position.
 *  -- punk1: ???
 *  -- punk2: ??? */
AsioError IAsio_GetSamplePosition(IAsio *This, uint64_t *unk1, uint64_t *unk2);
/* Gets channel info.
 *  -- pinfo: see definition of the AsioChannelInfo struct for input
 *            and output parameters */
AsioError IAsio_GetChannelInfo(IAsio *This, struct AsioChannelInfo *pinfo);
/* Create buffers for playback/recording.
 *  -- bufs: pointer to array of AsioBuffer structures
 *           see definition of AsioBuffers struct for input/output params
 *  -- numbufs: number of elements in the array pointed to by 'bufs'
 *  -- buffer_size: the requested buffer size.
 *                  this must be within the bounds given by GetBufferSize
 *  -- cbs: pointer to a structure initialized with pointers to callback
 *          functions as described in the definition of
 *          'AsioCreateBufferCallbacks'.
 * WARNING: the 'cbs' pointer MUST be valid for the entirety of the lifetime
 * of the IAsio. Many drivers do NOT copy the data inside the pointer, but
 * simply copy the pointer itself. You should probably store this data as
 * a global or alongside the IAsio. */
AsioError IAsio_CreateBuffers(IAsio *This, struct AsioBuffers *bufs, uint32_t numbufs, uint32_t buffer_size, struct AsioCreateBufferCallbacks *cbs);
/* Destroys buffers previously allocated with IAsio_CreateBuffers. */
AsioError IAsio_DestroyBuffers(IAsio *This);
/* Opens the driver control panel. */
AsioError IAsio_ControlPanel(IAsio *This);
/* Polls for future additions to the driver. */
AsioError IAsio_Future(IAsio *This, uint32_t which);
/* Notifies the driver that any output buffers are ready for processing */
AsioError IAsio_OutputReady(IAsio *This);

/* ------------------------------------------------------------------------ */
/* Driver listing */

/* Polls for drivers */
void Asio_DriverPoll(void);

/* Returns the current driver count.
 * Note that if you don't call Asio_DriverPoll() before this,
 * this function will return 0. */
uint32_t Asio_DriverCount(void);
/* This pointer is only guaranteed to be valid UNTIL the next call
 * into Asio_DriverPoll.
 * Hopefully no one needs two parts of their program to interact
 * with this... */
const char *Asio_DriverDescription(uint32_t x);
/* OS-specific poll for the driver.
 * This returns the pointer to the ASIO interface, but it is *not*
 * actually initialized yet. You must call IAsio_Init() before doing
 * anything important. */
IAsio *Asio_DriverGet(uint32_t x);

/* ------------------------------------------------------------------------ */
/* Library initialization functions */

int32_t Asio_Init(void);
void Asio_Quit(void);

#endif /* ASIO_INCL_H_ */
