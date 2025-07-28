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

/* This is the layout of the ASIO vtable.
 *
 * There are no header guards here -- this is by design, as it's meant to be
 * #include'd multiple times by different files to prevent having to
 * redefine everything over and over.
 *
 * Some function parameters are still undocumented. I prevented myself from
 * looking into existing code (beyond said disassemblies that gave hints to
 * function parameters, and some array bounds). */

// #define ASIO_FUNC(type, name, paramswtype, params, calltype)

/* standard IUnknown functions, these are documented enough already */
ASIO_FUNC(HRESULT, QueryInterface, (IAsio *This, REFIID riid, void **ppvObject), (This, riid, ppvObject), __stdcall);
ASIO_FUNC(ULONG, AddRef, (IAsio *This), (This), __stdcall);
ASIO_FUNC(ULONG, Release, (IAsio *This), (This), __stdcall);

/* initializes the driver
 * TODO: wtf is unk1 */
ASIO_FUNC(AsioError, Init, (IAsio *This, void *unk1), (This, unk1), __thiscall);

/* receives the driver name
 * [out] name: the buffer to fill with the name of the driver.
 *       max size is 32 apparently, FlexASIO calls strcpy_s with
 *       length 0x20. */
ASIO_FUNC(void, GetDriverName, (IAsio *This, char name[32]), (This, name), __thiscall);

/* returns the driver version.
 * (source: ASIO4ALL v2 returns 2. lol) */
ASIO_FUNC(uint32_t, GetDriverVersion, (IAsio *This), (This), __thiscall);

/* receives the last error message.
 * [out] msg: the buffer to fill with the error message. array bounds taken from
 *            a strcpy_s within ASIO2WASAPI. */
ASIO_FUNC(void, GetErrorMessage, (IAsio *This, char msg[128]), (This, msg), __thiscall);

/* starts playback */
ASIO_FUNC(AsioError, Start, (IAsio *This), (This), __thiscall);

/* stops playback */
ASIO_FUNC(AsioError, Stop, (IAsio *This), (This), __thiscall);

/* grabs the amount of channels
 * [out] pinchns: retrieves the number of input channels
 * [out] poutchns: retrieves the number of output channels */
ASIO_FUNC(AsioError, GetChannels, (IAsio *This, uint32_t *pinchns, uint32_t *poutchns), (This, pinchns, poutchns), __thiscall);

/* grabs the latency(??) for the driver
 * I highly doubt these values are trustworthy.
 * [out] pinlatency: retrieves the latency for input channels in frames
 * [out] poutlatency: retrieves the latency for output channels in frames */
ASIO_FUNC(AsioError, GetLatencies, (IAsio *This, uint32_t *pinlatency, uint32_t *poutlatency), (This, pinlatency, poutlatency), __thiscall);

/* Grabs the buffer sizes:
 * [out] pmin: receives the minimum buffer size
 * [out] pmax: receives the maximum buffer size
 * [out] pmin: receives the preferred buffer size
 * [out] punk: receives something unknown (TODO) */
ASIO_FUNC(AsioError, GetBufferSize, (IAsio *This, uint32_t *pmin, uint32_t *pmax, uint32_t *pwanted, uint32_t *punknown), (This, pmin, pmax, pwanted, punknown), __thiscall);

/* Check whether a driver can play a certain sample rate.
 * [in] rate: the sample rate to check */
ASIO_FUNC(AsioError, CheckSampleRate, (IAsio *This, double rate), (This, rate), __thiscall);

/* Gets the sample rate the driver is currently playing at.
 * [out] prate: pointer to double receiving the sample rate */
ASIO_FUNC(AsioError, GetSampleRate, (IAsio *This, double *prate), (This, prate), __thiscall);

/* Sets the sample rate the driver plays at.
 * [in] rate: the sampling rate in question */
ASIO_FUNC(AsioError, SetSampleRate, (IAsio *This, double rate), (This, rate), __thiscall);

/* Receives the clock sources.
 * [out]      srcs: an array of clock source structures to be filled in by the
 *            callee.
 * [in, out]: size: on input, the number of allocated structures pointed to by
 *            srcs. on output, the actual number of structures that were filled
 *            in. */
ASIO_FUNC(AsioError, GetClockSources, (IAsio *This, struct AsioClockSource *srcs, uint32_t *size), (This, srcs, size), __thiscall);
ASIO_FUNC(AsioError, SetClockSource, (IAsio *This, uint32_t src), (This, src), __thiscall);

/* Gets the current sample position.
 * [out] unk1: TODO, 8-bytes long
 * [out] unk2: TODO, 8-bytes long */
ASIO_FUNC(AsioError, GetSamplePosition, (IAsio *This, uint64_t *unk1, uint64_t *unk2), (This, unk1, unk2), __thiscall);

/* retrieves info about a channel.
 * [in, out] 'pinfo': caller should fill in members marked "INPUT" in the
 *           structure definition, and the callee will fill in the remaining
 *           members. */
ASIO_FUNC(AsioError, GetChannelInfo, (IAsio *This, struct AsioChannelInfo *pinfo), (This, pinfo), __thiscall);

/* buffer management.
 * [in, out] 'bufs': an array of buffers. the caller should fill in the
 *           members marked "INPUT" in the structure definition (see
 *           audio-asio.h), and the callee will fill the remaining members.
 *           do note that each channel has its own separate buffer.
 * [in]      'numbufs': number of structures allocated in 'bufs'
 * [in]      'buffer_size': size of the buffers to be placed in 'bufs' in samples
 * [in]      'cbs': a pointer to a structure containing four consecutive function
 *           pointers that the driver calls. these functions are documented
 *           within the structure definition in audio-asio.h.
 *           NOTE: this callback structure MUST be retained while the buffers
 *           are in use by the driver (e.g. before DestroyBuffers()), because
 *           some drivers expect the pointer to stay valid after CreateBuffers
 *           returns. */
ASIO_FUNC(AsioError, CreateBuffers, (IAsio *This, struct AsioBuffers *bufs, uint32_t numbufs, uint32_t buffer_size, struct AsioCreateBufferCallbacks *cbs), (This, bufs, numbufs, buffer_size, cbs), __thiscall);
ASIO_FUNC(AsioError, DestroyBuffers, (IAsio *This), (This), __thiscall);

/* Opens up the driver control panel, which allows the user to
 * configure driver-specific settings. */
ASIO_FUNC(AsioError, ControlPanel, (IAsio *This), (This), __thiscall);

/* this is some kind of handler for adding future things into ASIO;
 * I'm guessing this "polls" for support of a certain thing?
 * FlexASIO aborts, FL Studio ASIO simply returns some values for some inputs. */
ASIO_FUNC(AsioError, Future, (IAsio *This, uint32_t which), (This, which), __thiscall);

/* call this every time you've filled the buffer */
ASIO_FUNC(AsioError, OutputReady, (IAsio *This), (This), __thiscall);

#undef ASIO_FUNC
