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

/* Mac OS ASIO functions */

#include "audio-asio.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <Files.h>
#include <CodeFragments.h>
#include <Resources.h>
#include <TextUtils.h>
#include <Folders.h>
#include <Processes.h>

/* Macintosh ASIO structure.. */
struct IAsio {
	Handle handle;
	CFragConnectionID conn;
	short resfile;

	/* Function pointers loaded from the driver */
	AsioError (*Init)(void *unk1);
	AsioError (*Quit)(void);
	AsioError (*Start)(void);
	AsioError (*Stop)(void);
	AsioError (*GetChannels)(uint32_t *pinchns, uint32_t *poutchns);
	AsioError (*GetLatencies)(uint32_t *pinlatency, uint32_t *poutlatency);
	AsioError (*GetBufferSize)(uint32_t *pmin, uint32_t *pmax, uint32_t *pwanted, uint32_t *punknown);
	AsioError (*CheckSampleRate)(double rate);
	AsioError (*GetSampleRate)(double *prate);
	AsioError (*SetSampleRate)(double rate);
	AsioError (*GetClockSources)(struct AsioClockSource *srcs, uint32_t *size);
	AsioError (*SetClockSource)(uint32_t src);
	AsioError (*GetSamplePosition)(uint64_t *unk1, uint64_t *unk2);
	AsioError (*GetChannelInfo)(struct AsioChannelInfo *pinfo);
	AsioError (*CreateBuffers)(struct AsioBuffers *bufs, uint32_t numbufs, uint32_t buffer_size, struct AsioCreateBufferCallbacks *cbs);
	AsioError (*DestroyBuffers)(void);
	AsioError (*ControlPanel)(void);
	AsioError (*Future)(uint32_t which);
	AsioError (*OutputReady)(void);

	uint32_t ver; /* cached on init */
	char name[32];
	char errmsg[128];
};

static struct asio_device {
	FSSpec spec;
	char *description; /* UTF-8 */
} *drivers = NULL;
static size_t drivers_size = 0;
static size_t drivers_alloc = 0;

static void Asio_Mac_ClearDrivers(void)
{
	/* Clears the drivers list, but keeps the memory */
	size_t i;

	for (i = 0; i < drivers_size; i++)
		free(drivers[i].description);

	drivers_size = 0;
}

static void Asio_Mac_FreeDrivers(void)
{
	/* Clear the list before anything else */
	Asio_Mac_ClearDrivers();

	/* Now free the actual array and reset everything to defaults */
	free(drivers);

	drivers = NULL;
	drivers_alloc = 0;
}

static int Asio_Mac_PathToFSSpec(const char *path, FSSpec *spec)
{
	size_t len;
	unsigned char ppath[256];

	len = strlen(path);
	if (len > 255)
		return 0;

	ppath[0] = len;
	memcpy(ppath + 1, path, len);

	return (FSMakeFSSpec(0, 0, ppath, spec) == noErr);
}

/* Grabs the current executable's path, and stores it in the
 * FSSpec pointed to by 'spec' */
static int Asio_Mac_GetExecutablePath(FSSpec *spec)
{
	ProcessSerialNumber process;
	ProcessInfoRec process_info;

	process.highLongOfPSN = 0;
	process.lowLongOfPSN = kCurrentProcess;
	process_info.processInfoLength = sizeof(process_info);
	process_info.processName = NULL;
	process_info.processAppSpec = spec;

	if (GetProcessInformation(&process, &process_info) != noErr)
		return -1;

	return 0;
}

/* Modifies 'spec' in-place to point to the parent
 * directory of whatever was already in it. */
static int Asio_Mac_GetParentPath(FSSpec *spec)
{
	CInfoPBRec cinfo;

	cinfo.dirInfo.ioNamePtr = spec->name;
	cinfo.dirInfo.ioVRefNum = spec->vRefNum;
	cinfo.dirInfo.ioFDirIndex = -1;
	cinfo.dirInfo.ioDrDirID = spec->parID;

	if (PBGetCatInfoSync(&cinfo) != noErr)
		return -1;

	spec->parID = cinfo.dirInfo.ioDrParID;
	return 0;
}

/* 'len' can be zero, in which case strlen is called on data */
static int Asio_Mac_PascalStrAppend(unsigned char pstr[256],
	const char *data, size_t len)
{
	if (!len)
		return 0; /* do nothing */

	if ((pstr[0] + len) > 255)
		return -1;

	memcpy(pstr + 1 + pstr[0], data, len);
	pstr[0] += len;
	return 0;
}

/* Specialization of PascalStrAppend for pascal strings as
 * a source */
static int Asio_Mac_PascalStrAppendP(unsigned char pdst[256],
	const unsigned char psrc[256])
{
	return Asio_Mac_PascalStrAppend(pdst, psrc + 1, psrc[0]);
}

/* Copy data into a Pascal string */
static int Asio_Mac_PascalStrCopy(unsigned char pstr[256],
	const char *data, size_t len)
{
	pstr[0] = 0;
	return Asio_Mac_PascalStrAppend(pstr, data, len);
}

/* Convert a Pascal string to a C string on the heap,
 * and return the result */
static char *Asio_Mac_PascalStrDup(unsigned char *pstr)
{
	char *q = malloc(pstr[0] + 1);
	if (q) {
		memcpy(q, pstr + 1, pstr[0]);
		q[pstr[0]] = 0;
	}
	return q;
}

void Asio_DriverPoll(void)
{
	char DriverDirName[] = {
		13, /* length */
		':', /* partial path */
		'A', 'S', 'I', 'O', ' ',
		'D', 'r', 'i', 'v', 'e', 'r', 's'
	};
	FSSpec spec;
	WDPBRec pb;

	/* Free any drivers we have already, but keep the allocation
	 * for the array, as it probably won't change. */
	Asio_Mac_ClearDrivers();

	if (Asio_Mac_GetExecutablePath(&spec) < 0)
		return;

	pb.ioNamePtr = DriverDirName;
	pb.ioVRefNum = spec.vRefNum;
	pb.ioWDDirID = spec.parID;
	pb.ioWDProcID = 0; /* I don't care */

	if (PBOpenWD(&pb, 0) != noErr)
		return;

	{
		CInfoPBRec pbr;
		unsigned char name[256];

		pbr.dirInfo.ioNamePtr = name;
		pbr.dirInfo.ioVRefNum = pb.ioVRefNum;
		pbr.dirInfo.ioDrDirID = 0;
		pbr.dirInfo.ioFDirIndex = 1;

		for (;;) {
			struct asio_device *dev;

			name[0] = 0;

			if (PBGetCatInfo(&pbr, 0) != noErr)
				break; /* done */

			/* Append it to our drivers list.
			 * If we don't have any drivers, this will just
			 * allocate 4 (probably more than enough anyway) */
			if (drivers_size >= drivers_alloc) {
				drivers_alloc = (drivers_alloc)
					? (drivers_alloc * 2)
					/* sane default value I guess */
					: 4;

				dev = realloc(drivers, drivers_alloc * sizeof(struct asio_device));
				if (!dev)
					break; /* ...we're probably screwed... */
			}

			dev = drivers + drivers_size;

			if (FSMakeFSSpec(pb.ioVRefNum, 0, name, &dev->spec) != noErr)
				continue; /* NO FREAKING WAY */

			/* FIXME: To do this properly, we have to
			 *  1. open the resource fork
			 *  2. read in the whole 'Asio' resource
			 *  3. call GetResInfo to get the name
			 *  4. convert that name to UTF-8
			 * This will be okay for now though. */
			dev->description = Asio_Mac_PascalStrDup(name);

			/* Increment this */
			pbr.dirInfo.ioFDirIndex++;
			/* and this */
			drivers_size++;
		}
	}

	(void)PBCloseWD(&pb, 0);
}

uint32_t Asio_DriverCount(void)
{
	return drivers_size;
}

const char *Asio_DriverDescription(uint32_t x)
{
	if (x >= drivers_size)
		return NULL;

	return drivers[x].description;
}

/* TODO when this was apart of schism it used to print out some
 * debugging information.
 *
 * It would be nice to find some way to reintroduce that */
IAsio *Asio_DriverGet(uint32_t x)
{
	/* I'm not *really* happy with this. */
	IAsio *asio;
	OSErr err;
	short resfile;
	short oldresfile;
	Handle res;
	uint32_t (*mainfunc)(void);
	Str255 errname;
	CFragConnectionID conn;

	if (x >= drivers_size)
		return NULL;

	resfile = FSpOpenResFile(&drivers[x].spec, fsRdPerm);
	if (resfile < 0)
		return NULL;

	/* back this up... */
	oldresfile = CurResFile();

	UseResFile(resfile);

	/* TODO we shouldn't have to keep the resource file around
	 * after this; see DetachResource docs.
	 *
	 * However, I'm keeping it around since the docs aren't very
	 * clear on how the memory is supposed to be released once
	 * detached from the Resource Manager. */
	res = GetResource('Asio', 0);

	/* restore it before doing anytihng else */
	UseResFile(oldresfile);

	if (!res) {
		/* handle it */
		CloseResFile(resfile);
		return NULL;
	}
	HLock(res);
	err = GetMemFragment(*res, GetHandleSize(res),
		"\xd" "ASIO fragment", kReferenceCFrag, &conn,
		(Ptr *)&mainfunc, errname);
	HUnlock(res);

	/* We can do a little bit less redundant cleanup
	 * by allocating this here and checking for all of the
	 * failure conditions next */
	asio = calloc(1, sizeof(*asio));

	if ((err != noErr)
			|| (mainfunc && mainfunc() != 0x4153494F /* 'ASIO' */)
			|| !asio) {
		free(asio);
		ReleaseResource(res);
		CloseResFile(resfile);
		return NULL;
	}

#define LOAD_SYMBOL(name, namemangled) \
do { \
	unsigned char pname[256]; \
	CFragSymbolClass cls; \
\
	/* TODO check 'cls' and make sure we have the Right Thing */ \
	Asio_Mac_PascalStrCopy(pname, #namemangled, sizeof(#namemangled) - 1); \
	err = FindSymbol(conn, pname, (Ptr *)&asio->name, &cls); \
\
	if (err != noErr) { \
		free(asio); \
		CloseConnection(&conn); \
		ReleaseResource(res); \
		CloseResFile(resfile); \
		return NULL; \
	} \
} while (0)

	LOAD_SYMBOL(OutputReady, ASIOOutputReady__Fv);
	LOAD_SYMBOL(Future, ASIOFuture__FlPv);
	LOAD_SYMBOL(ControlPanel, ASIOControlPanel__Fv);
	LOAD_SYMBOL(DestroyBuffers, ASIODisposeBuffers__Fv);
	LOAD_SYMBOL(CreateBuffers, ASIOCreateBuffers__FP14ASIOBufferInfollP13ASIOCallbacks);
	LOAD_SYMBOL(GetChannelInfo, ASIOGetChannelInfo__FP15ASIOChannelInfo);
	LOAD_SYMBOL(GetSamplePosition, ASIOGetSamplePosition__FP11ASIOSamplesP13ASIOTimeStamp);
	LOAD_SYMBOL(SetClockSource, ASIOSetClockSource__Fl);
	LOAD_SYMBOL(GetClockSources, ASIOGetClockSources__FP15ASIOClockSourcePl);
	LOAD_SYMBOL(SetSampleRate, ASIOSetSampleRate__Fd);
	LOAD_SYMBOL(GetSampleRate, ASIOGetSampleRate__FPd);
	LOAD_SYMBOL(CheckSampleRate, ASIOCanSampleRate__Fd);
	LOAD_SYMBOL(GetBufferSize, ASIOGetBufferSize__FPlPlPlPl);
	LOAD_SYMBOL(GetLatencies, ASIOGetLatencies__FPlPl);
	LOAD_SYMBOL(GetChannels, ASIOGetChannels__FPlPl);
	LOAD_SYMBOL(Stop, ASIOStop__Fv);
	LOAD_SYMBOL(Start, ASIOStart__Fv);
	LOAD_SYMBOL(Quit, ASIOExit__Fv);
	LOAD_SYMBOL(Init, ASIOInit__FP14ASIODriverInfo);

#undef LOAD_SYMBOL

	asio->handle = res;
	asio->conn = conn;
	asio->resfile = resfile;

	return (IAsio *)asio;
}

/* ------------------------------------------------------------------------ */
/* IAsio functions */

AsioError IAsio_Init(IAsio *This)
{
	AsioError err;
	unsigned char fake[168] __attribute__((__aligned__(4)));

	/* Set everything to zero
	 * This fixes weird issues. Something's definitely using whatever
	 * is getting put in ASIO_Init() as an input. */
	memset(fake, 0, sizeof(fake));

	err = This->Init(fake);

	/* driver version is stored 4 bytes in */
	memcpy(&This->ver, fake + 4, 4);
	/* driver name is stored directly after */
	memcpy(This->name, fake + 8, 32);
	/* error message is stored directly after */
	memcpy(This->errmsg, fake + 40, 128);

	return err;
}

void IAsio_Quit(IAsio *This)
{
	/* kill it all off! */
	This->Quit();
	CloseConnection(&This->conn);
	ReleaseResource(This->handle);
	CloseResFile(This->resfile);
	free(This);
}

void IAsio_GetDriverName(IAsio *This, char name[32])
{
	strncpy(name, This->name, 31);
	name[31] = 0;
}

uint32_t IAsio_GetDriverVersion(IAsio *This)
{
	return This->ver;
}

void IAsio_GetErrorMessage(IAsio *This, char msg[128])
{
	strncpy(msg, This->errmsg, 127);
	msg[127] = 0;
}

AsioError IAsio_Start(IAsio *This)
{
	return This->Start();
}

AsioError IAsio_Stop(IAsio *This)
{
	return This->Stop();
}

AsioError IAsio_GetChannels(IAsio *This, uint32_t *pinchns, uint32_t *poutchns)
{
	return This->GetChannels(pinchns, poutchns);
}

AsioError IAsio_GetLatencies(IAsio *This, uint32_t *pinlatency, uint32_t *poutlatency)
{
	return This->GetLatencies(pinlatency, poutlatency);
}

AsioError IAsio_GetBufferSize(IAsio *This, uint32_t *pmin, uint32_t *pmax, uint32_t *pwanted, uint32_t *punknown)
{
	return This->GetBufferSize(pmin, pmax, pwanted, punknown);
}

AsioError IAsio_SupportsSampleRate(IAsio *This, double rate)
{
	return This->CheckSampleRate(rate);
}

AsioError IAsio_GetSampleRate(IAsio *This, double *prate)
{
	return This->GetSampleRate(prate);
}

AsioError IAsio_SetSampleRate(IAsio *This, double rate)
{
	return This->SetSampleRate(rate);
}

AsioError IAsio_GetClockSources(IAsio *This, struct AsioClockSource *srcs, uint32_t *size)
{
	return This->GetClockSources(srcs, size);
}

AsioError IAsio_SetClockSource(IAsio *This, uint32_t src)
{
	return This->SetClockSource(src);
}

AsioError IAsio_GetSamplePosition(IAsio *This, uint64_t *unk1, uint64_t *unk2)
{
	return This->GetSamplePosition(unk1, unk2);
}

AsioError IAsio_GetChannelInfo(IAsio *This, struct AsioChannelInfo *pinfo)
{
	return This->GetChannelInfo(pinfo);
}

AsioError IAsio_CreateBuffers(IAsio *This, struct AsioBuffers *bufs, uint32_t numbufs, uint32_t buffer_size, struct AsioCreateBufferCallbacks *cbs)
{
	return This->CreateBuffers(bufs, numbufs, buffer_size, cbs);
}

AsioError IAsio_DestroyBuffers(IAsio *This)
{
	return This->DestroyBuffers();
}

AsioError IAsio_ControlPanel(IAsio *This)
{
	return This->ControlPanel();
}

AsioError IAsio_Future(IAsio *This, uint32_t which)
{
	return This->Future(which);
}

AsioError IAsio_OutputReady(IAsio *This)
{
	return This->OutputReady();
}

int32_t Asio_Init(void) { return 0; }
void Asio_Quit(void)
{
	Asio_Mac_FreeDrivers();
}
