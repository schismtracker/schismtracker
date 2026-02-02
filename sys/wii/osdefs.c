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
#include "osdefs.h"
#include "song.h"
#include "it.h" // need for kbd_get_alnum
#include "log.h"
#include "dmoz.h"
#include "util.h"
#include "mem.h"

#include <di/di.h>
#include <fat.h>
#include <ogc/machine/processor.h>
#include <ogc/system.h>
#include <ogc/es.h>
#include <ogc/ios.h>
#include <ogc/usbmouse.h>
#include <ogcsys.h>
#include <wiikeyboard/keyboard.h>
#include <wiiuse/wpad.h>
#include <sys/dir.h>
#include <dirent.h>
#include "isfs.h"
#include <fat.h>

/*
Turn this on to bypass SDL weak-linking and allow OpenGL to be used.
Currently this is disabled, because the implementation isn't very
good and causes noticeable artifacting on the display. Plus it's
SUPER slow compared to a direct blit, even in software.

#define WII_USE_OPENGX 1
*/

#ifdef WII_USE_OPENGX
# include <opengx.h>
#endif

// cargopasta'd from libogc git __di_check_ahbprot
static u32 _check_ahbprot(void)
{
	s32 res;
	u64 title_id;
	u32 tmd_size;
	STACK_ALIGN(u32, tmdbuf, 1024, 32);

	res = ES_GetTitleID(&title_id);
	if (res < 0) {
		log_appendf(4, "ES_GetTitleID() failed: %d", res);
		return res;
	}

	res = ES_GetStoredTMDSize(title_id, &tmd_size);
	if (res < 0) {
		log_appendf(4, "ES_GetStoredTMDSize() failed: %d", res);
		return res;
	}

	if (tmd_size > 4096) {
		log_appendf(4, "TMD too big: %d", tmd_size);
		return -EINVAL;
	}

	res = ES_GetStoredTMD(title_id, tmdbuf, tmd_size);
	if (res < 0) {
		log_appendf(4, "ES_GetStoredTMD() failed: %d", res);
		return -EINVAL;
	}

	if ((tmdbuf[0x76] & 3) == 3) {
		return 1;
	}

	return 0;
}

#if defined(SCHISM_SDL2)
/* UGH */
extern bool OGC_ResetRequested, OGC_PowerOffRequested;

static void ShutdownCB(void)
{
	OGC_PowerOffRequested = true;
}

static void ResetCB(SCHISM_UNUSED u32 irq, SCHISM_UNUSED void *ctx)
{
	OGC_ResetRequested = true;
}
#elif defined(SCHISM_SDL12)
extern void OGC_InitVideoSystem(void);

/* need to reimplement this too */
extern bool TerminateRequested, ShutdownRequested, ResetRequested;
bool TerminateRequested, ShutdownRequested, ResetRequested;

static void ShutdownCB(void)
{
	TerminateRequested = true;
	ShutdownRequested = true;
}

static void ResetCB(SCHISM_UNUSED u32 irq, SCHISM_UNUSED void *ctx)
{
	TerminateRequested = true;
	ResetRequested = true;
}

static void ShutdownWii(void)
{
	TerminateRequested = false;
	//SDL_Quit();
	SYS_ResetSystem(SYS_POWEROFF, 0, 0);
}

static void RestartHomebrewChannel(void)
{
	TerminateRequested = false;
	//SDL_Quit();
	schism_exit(1);
}

void Terminate(void)
{
	if (ShutdownRequested) ShutdownWii();
	else if (ResetRequested) RestartHomebrewChannel();
}
#else
static void ShutdownCB(void)
{
    schism_event_t e;
   	e.type = SCHISM_QUIT;
   	events_push_event(&e);
}

static void ResetCB(SCHISM_UNUSED u32 irq, SCHISM_UNUSED void *ctx)
{
	ShutdownCB();
}
#endif

static void ShutdownWPADCB(SCHISM_UNUSED s32 chan)
{
	ShutdownCB();
}

void wii_sysinit(int *pargc, char ***pargv)
{
	char *ptr = NULL;

#ifdef WII_USE_OPENGX
	/* dummy calls to get gcc to actually link with opengx */
	ogx_initialize();
	ogx_enable_double_buffering(1);
#endif

#if defined(SCHISM_SDL2) || defined(SCHISM_SDL12)
	{
		/* Even though this crap *should* be handled by SDL_Init, it is instead
		 * handled by SDL_main, which breaks our stuff. Sigh. */
		L2Enhance();

		{
			/* Reload into the preferred IOS */
			u32 version;
			s32 preferred;

			version = IOS_GetVersion();
			preferred = IOS_GetPreferredVersion();

			if (preferred > 0 && version != (u32)preferred)
				IOS_ReloadIOS(preferred);
		}

		WPAD_Init();
		WPAD_SetPowerButtonCallback(ShutdownWPADCB);
		SYS_SetPowerCallback(ShutdownCB);
		SYS_SetResetCallback(ResetCB);

#ifdef SCHISM_SDL12
		/* this is specific to SDL 1.2 */
		PAD_Init();
		/* why is this not in SDL_InitSubSystem ??? */
		OGC_InitVideoSystem();
#endif

		WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
		WPAD_SetVRes(WPAD_CHAN_ALL, 640, 480);

		MOUSE_Init();
		KEYBOARD_Init(NULL);
	}
#endif

	log_appendf(1, "[Wii] This is IOS%d v%X, and AHBPROT is %s",
		IOS_GetVersion(), IOS_GetRevision(), _check_ahbprot() > 0 ? "enabled" : "disabled");
	if (*pargc == 0 && *pargv == NULL) {
		// I don't know if any other loaders provide similarly broken environments
		log_appendf(1, "[Wii] Was I just bannerbombed? Prepare for crash at exit...");
	} else if (memcmp((const /*volatile ?*/ void *)0x80001804, "STUBHAXX", 8) == 0) {
		log_appendf(1, "[Wii] Hello, HBC user!");
	} else {
		log_appendf(1, "[Wii] Where am I?!");
	}

	// redirect stdout/stderr to dolphin OSReport uart, for ease of debugging
	SYS_STDIO_Report(true);

	ISFS_SU();
	if (ISFS_Initialize() == IPC_OK)
		ISFS_Mount();
	/* no idea what the "default" device entails */
	fatInitDefault();

	// Attempt to locate a suitable home directory.
	if (!*pargc || !*pargv) {
		// loader didn't bother setting these
		*pargc = 1;
		*pargv = mem_alloc(sizeof(char **) * 2);
		(*pargv)[0] = str_dup("?");
		(*pargv)[1] = NULL;
	} else if (strchr(*pargv[0], '/') != NULL) {
		// presumably launched from hbc menu - put stuff in the boot dir
		// (does get_parent_directory do what I want here?)
		ptr = dmoz_path_get_parent_directory(*pargv[0]);
	}
	if (!ptr) {
		// Make a guess anyway
		ptr = str_dup("sd:/apps/schismtracker");
	}
	if (chdir(ptr) != 0) {
		DIR* dir = opendir("sd:/");
		free(ptr);
		if (dir) {
			// Ok at least the sd card works, there's some other dysfunction
			closedir(dir);
			ptr = str_dup("sd:/");
		} else { // 0x800bb784 0x800bb788 0x800bbafc 0x800bbb00
			// Safe (but useless) default
			ptr = str_dup("isfs:/");
		}
		chdir(ptr); // Hope that worked, otherwise we're hosed
	}
	setenv("HOME", ptr, 1);
	free(ptr);
}

void wii_sysexit(void)
{
	ISFS_Deinitialize();
}
