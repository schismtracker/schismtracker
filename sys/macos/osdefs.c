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

#include "it.h"
#include "osdefs.h"
#include "events.h"
#include "song.h"
#include "page.h"
#include "widget.h"
#include "dmoz.h"
#include "charset.h"
#include "str.h"
#include "config.h"
#include "config-parser.h"

#include <Files.h>
#include <Folders.h>
#include <Dialogs.h>

#include <ctype.h>

/* ------------------------------------------------------------------------ */

static inline SCHISM_ALWAYS_INLINE time_t time_get_macintosh_epoch(void)
{
	// This assumes the epoch is in UTC. This is only always true for Mac OS
	// Extended partitions; I'm not sure about the time functions since I
	// don't use those...
    struct tm macintosh_epoch_tm = {
        .tm_year = 4, // 1904
        .tm_mon = 0,  // January
        .tm_mday = 1, // 1st
    };

    return mktime(&macintosh_epoch_tm);
}

static inline SCHISM_ALWAYS_INLINE time_t time_convert_from_macintosh(uint32_t x)
{
    return ((time_t)x) + time_get_macintosh_epoch();
}

static inline SCHISM_ALWAYS_INLINE uint32_t time_convert_to_macintosh(time_t x)
{
    return (uint32_t)(x - time_get_macintosh_epoch());
}

/* ------------------------------------------------------------------------ */

void macos_get_modkey(schism_keymod_t *mk)
{
	// SDL 1.2 treats Command as Control, so switch the values.
	schism_keymod_t mk_ = (*mk) & ~(SCHISM_KEYMOD_CTRL|SCHISM_KEYMOD_GUI);

	if (*mk & SCHISM_KEYMOD_LCTRL)
		mk_ |= SCHISM_KEYMOD_LGUI;

	if (*mk & SCHISM_KEYMOD_RCTRL)
		mk_ |= SCHISM_KEYMOD_RGUI;

	if (*mk & SCHISM_KEYMOD_LGUI)
		mk_ |= SCHISM_KEYMOD_LCTRL;

	if (*mk & SCHISM_KEYMOD_RGUI)
		mk_ |= SCHISM_KEYMOD_RCTRL;

	*mk = mk_;
}

/* ------------------------------------------------------------------------ */

void macos_show_message_box(const char *title, const char *text)
{
	// This converts the message to the HFS character set, which
	// isn't necessarily the system character set, but whatever
	unsigned char err[256], explanation[256];
	int16_t hit;

	{
		char *ntitle = charset_iconv_easy(title, CHARSET_UTF8, CHARSET_SYSTEMSCRIPT);
		str_to_pascal(ntitle, err, NULL);
		free(ntitle);
	}

	{
		char *ntext = charset_iconv_easy(text, CHARSET_UTF8, CHARSET_SYSTEMSCRIPT);
		str_to_pascal(ntext, explanation, NULL);
		free(ntext);
	}

	StandardAlert(kAlertDefaultOKText, err, explanation, NULL, &hit);
}

/* ------------------------------------------------------------------------ */

int macos_mkdir(const char *path, SCHISM_UNUSED mode_t mode)
{
	HParamBlockRec pb = {0};
	unsigned char mpath[256];
	struct stat st;

	{
		int truncated = 0;

		// Fix the path, then convert it to a pascal string
		char *normal = dmoz_path_normal(path);
		char *npath = charset_iconv_easy(normal, CHARSET_UTF8, CHARSET_SYSTEMSCRIPT);
		free(normal);
		str_to_pascal(npath, mpath, &truncated);
		free(npath);
		
		if (truncated) {
			errno = ENAMETOOLONG;
			return -1;
		}
	}

	// Append a separator on the end if one isn't there already; I don't
	// know if this is strictly necessary, but every macos path I've seen
	// that goes to a folder has an explicit path separator on the end.
	if (mpath[mpath[0]] != ':') {
		if (mpath[0] >= 255) {
			errno = ENAMETOOLONG;
			return -1;
		}
		mpath[++mpath[0]] = ':';
	}

	pb.fileParam.ioNamePtr = mpath;

	OSErr err = PBDirCreateSync(&pb);
	switch (err) {
	case noErr:
		return 0;
	case nsvErr:
	case fnfErr:
	case dirNFErr:
		errno = ENOTDIR;
		return -1;
	case dirFulErr:
	case dskFulErr:
		errno = ENOSPC;
		return -1;
	case bdNamErr:
		errno = EILSEQ;
		return -1;
	case ioErr:
	case wrgVolTypErr: //FIXME find a more appropriate errno value for this
		errno = EIO;
		return -1;
	case wPrErr:
	case vLckdErr:
		errno = EROFS;
		return -1;
	case afpAccessDenied:
		errno = EACCES;
		return -1;
	default:
		return -1;
	}
}

int macos_stat(const char *file, struct stat *st)
{
	CInfoPBRec pb = {0};
	unsigned char ppath[256];

	{
		int truncated;

		char *normal = dmoz_path_normal(file);
		char *npath = charset_iconv_easy(normal, CHARSET_UTF8, CHARSET_SYSTEMSCRIPT);
		free(normal);
		str_to_pascal(npath, ppath, &truncated);
		free(npath);

		if (truncated) {
			errno = ENAMETOOLONG;
			return -1;
		}
	}

	// If our path is just a volume name, PBGetCatInfoSync will
	// fail, so append a path separator on the end in this case.
	if (!memchr(ppath + 1, ':', ppath[0])) {
		if (ppath[0] >= 255) {
			errno = ENAMETOOLONG;
			return -1;
		}
		ppath[++ppath[0]] = ':';
	}

	if (!strcmp(file, ".")) {
		*st = (struct stat){
			.st_mode = S_IFDIR,
			.st_ino = -1,
		};
	} else {		
		pb.hFileInfo.ioNamePtr = ppath;

		OSErr err = PBGetCatInfoSync(&pb);
		switch (err) {
		case noErr:
			*st = (struct stat){
				.st_mode = (pb.hFileInfo.ioFlAttrib & ioDirMask) ? S_IFDIR : S_IFREG,
				.st_ino = pb.hFileInfo.ioFlStBlk,
				.st_dev = pb.hFileInfo.ioVRefNum,
				.st_nlink = 1,
				.st_size = pb.hFileInfo.ioFlLgLen,
				.st_atime = time_convert_from_macintosh(pb.hFileInfo.ioFlMdDat),
				.st_mtime = time_convert_from_macintosh(pb.hFileInfo.ioFlMdDat),
				.st_ctime = time_convert_from_macintosh(pb.hFileInfo.ioFlCrDat),
			};

			return 0;
		case nsvErr:
		case fnfErr:
			errno = ENOENT;
			return -1;
		case bdNamErr:
		case paramErr:
			errno = EILSEQ;
			return -1;
		case ioErr:
			errno = EIO;
			return -1;
		case afpAccessDenied:
			errno = EACCES;
			return -1;
		case dirNFErr:
		case afpObjectTypeErr:
			errno = ENOTDIR;
			return -1;
		default:
			return -1;
		}
	}

	return -1;
}

FILE *macos_fopen(const char* path, const char* flags)
{
	char *npath;

	{
		char *normal = dmoz_path_normal(path);
		npath = charset_iconv_easy(normal, CHARSET_UTF8, CHARSET_SYSTEMSCRIPT);
		free(normal);
	}

	if (!npath)
		return NULL;

	FILE* ret = fopen(npath, flags);

	free(npath);

	return ret;
}

/* ------------------------------------------------------------------------ */
/* MacOS initialization code stolen from SDL and removed Mac OS X code */

/*
	SDL - Simple DirectMedia Layer
	Copyright (C) 1997-2006 Sam Lantinga

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

	Sam Lantinga
	slouken@libsdl.org
*/

/* This file takes care of command line argument parsing, and stdio redirection
   in the MacOS environment. (stdio/stderr is *not* directed for Mach-O builds)
 */

#include <Dialogs.h>
#include <Fonts.h>
#include <Events.h>
#include <Resources.h>
#include <Folders.h>

/* The standard output files */
#define STDOUT_FILE	"stdout.txt"
#define STDERR_FILE	"stderr.txt"

/* Structure for keeping prefs in 1 variable */
typedef struct {
	Str255 command_line;
	Str255 video_driver_name;
	int    output_to_file;
} PrefsRecord;

/* See if the command key is held down at startup */
static int CommandKeyIsDown(void)
{
	KeyMap theKeyMap;

	GetKeys(theKeyMap);

	// FIXME why is this checking 6? that's not even the
	// proper scancode value for the command key
	return !!(((unsigned char *)theKeyMap)[6] & 0x80);
}

/* Parse a command line buffer into arguments */
static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	int argc;

	argc = 0;
	for (bufp = cmdline; *bufp; /* none */) {
		/* Skip leading whitespace */
		while (isspace(*bufp))
			++bufp;

		/* Skip over argument */
		if (*bufp == '"') {
			++bufp;
			if (*bufp) {
				if (argv)
					argv[argc] = bufp;

				++argc;
			}
			/* Skip over word */
			while (*bufp && (*bufp != '"'))
				++bufp;
		} else {
			if (*bufp) {
				if (argv)
					argv[argc] = bufp;

				++argc;
			}

			/* Skip over word */
			while (*bufp && !isspace(*bufp))
				++bufp;
		}
		if (*bufp ) {
			if (argv)
				*bufp = '\0';

			++bufp;
		}
	}

	if (argv)
		argv[argc] = NULL;

	return argc;
}

static inline SCHISM_ALWAYS_INLINE void cleanup_output_file(FILE *fp, const char *name)
{
	long ft = ftell(fp);

	fclose(fp);

	/* If fp has not changed, nothing will happen, since
	 * ftell will fail and return <0. In addition we don't
	 * want to delete anything possibly valuable, so no
	 * deleting if it's over 0. */
	if (!ft)
		remove(name);
}

/* Remove the output files if there was no output written */
static void cleanup_output(void)
{
	FILE *file;
	int empty;

	/* Flush the output in case anything is queued */
	fflush(stdout);
	fflush(stderr);

	cleanup_output_file(stdout, STDOUT_FILE);
	cleanup_output_file(stderr, STDERR_FILE);
}

// this is duplicated in dmoz.c, whatever
static int get_app_file_name(char name[64])
{
	ProcessSerialNumber process;
	ProcessInfoRec      process_info;
	FSSpec              process_fsp;

	// ok
	name[0] = '\0';

	process.highLongOfPSN = 0;
	process.lowLongOfPSN  = kCurrentProcess;
	process_info.processInfoLength = sizeof (process_info);
	process_info.processName    = NULL;
	process_info.processAppSpec = &process_fsp;

	if (GetProcessInformation(&process, &process_info) != noErr)
	   return 0;

	/* should never ever happen */
	assert(process_fsp.name[0] < 64);

	/* bogus warning here */
	str_from_pascal(process_fsp.name, name);

	return 1;
}

/* Hacked together from older crap */
static void macos_cfg_load(PrefsRecord *prefs)
{
	char *tmp;
	cfg_file_t cfg;

	tmp = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, tmp);
	free(tmp);

	/* I guess it's okay to save these in system encoding, since they're not touched by anything else anyway */
	str_to_pascal(cfg_get_string(&cfg, "MacOS", "command_line", NULL, 0, ""), prefs->command_line, NULL);
	str_to_pascal(cfg_get_string(&cfg, "MacOS", "video_driver_name", NULL, 0, ""), prefs->video_driver_name, NULL);
	prefs->output_to_file = cfg_get_number(&cfg, "MacOS", "output_to_file", 1);

	cfg_free(&cfg);
}

static void macos_cfg_save(PrefsRecord *prefs)
{
	char buf[256];
	char *tmp;
	cfg_file_t cfg;

	tmp = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, tmp);
	free(tmp);

	str_from_pascal(prefs->command_line, buf);
	cfg_set_string(&cfg, "MacOS", "command_line", buf);

	str_from_pascal(prefs->video_driver_name, buf);
	cfg_set_string(&cfg, "MacOS", "video_driver_name", buf);

	cfg_set_number(&cfg, "MacOS", "output_to_file", prefs->output_to_file);

	cfg_write(&cfg);
	cfg_free(&cfg);
}

static char **args = NULL;

#define DEFAULT_ARGUMENTS ""
#define DEFAULT_VIDEO_DRIVER "toolbox"
#define DEFAULT_REDIRECT_STDIO 1

/* called by main; */
void macos_sysinit(int *pargc, char ***pargv)
{
	PrefsRecord prefs = {0};
	size_t i;
	int settingsChanged = 0, nargs;
	char commandLine[256];
	enum {
		/* these correspond to popup menu choices */
		VIDEO_ID_DRAWSPROCKET = 1,
		VIDEO_ID_TOOLBOX = 2,
	} videodriver = VIDEO_ID_TOOLBOX;

	/* ok */
	dmoz_init();

	cfg_init_dir();

	str_to_pascal(DEFAULT_ARGUMENTS, prefs.command_line, NULL);
	str_to_pascal(DEFAULT_VIDEO_DRIVER, prefs.video_driver_name, NULL);
	prefs.output_to_file = DEFAULT_REDIRECT_STDIO;

	/* Kyle's SDL command-line dialog code ... */
	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	InitDialogs(nil);
	InitCursor();
	InitContextualMenus();
	FlushEvents(everyEvent,0);
	MaxApplZone();
	MoreMasters();
	MoreMasters();

	macos_cfg_load(&prefs);

	if (memcmp(prefs.video_driver_name+1, "DSp", 3) == 0) {
		videodriver = VIDEO_ID_DRAWSPROCKET;
	} else if (memcmp(prefs.video_driver_name+1, "toolbox", 7) == 0) {
		videodriver = VIDEO_ID_TOOLBOX;
	} else {
		/* Where are we now ? */
	}
		
	if (CommandKeyIsDown()) {
		/* enum-ified  --paper */
		enum {
			kCL_OK = 1,
			kCL_Cancel = 2,
			kCL_Text = 3,
			kCL_File = 4,
			kCL_Video = 6,
		};

		DialogPtr commandDialog;
		short dummyType;
		Rect dummyRect;
		Handle dummyHandle;
		short itemHit;

		/* Assume that they will change settings, rather than do exhaustive check */
		settingsChanged = 1;

		/* Create dialog and display it */
		commandDialog = GetNewDialog(1000, nil, (WindowPtr)-1);
		SetPort(commandDialog);

		/* Setup controls */
		GetDialogItem(commandDialog, kCL_File, &dummyType, &dummyHandle, &dummyRect); /* MJS */
		SetControlValue((ControlHandle)dummyHandle, prefs.output_to_file);

		GetDialogItem(commandDialog, kCL_Text, &dummyType, &dummyHandle, &dummyRect);
		SetDialogItemText(dummyHandle, prefs.command_line);

		GetDialogItem(commandDialog, kCL_Video, &dummyType, &dummyHandle, &dummyRect);
		SetControlValue((ControlRef)dummyHandle, videodriver);

		SetDialogDefaultItem(commandDialog, kCL_OK);
		SetDialogCancelItem(commandDialog, kCL_Cancel);

		do {
			ModalDialog(nil, &itemHit); /* wait for user response */
			
			/* Toggle command-line output checkbox */	
			if (itemHit == kCL_File) {
				GetDialogItem(commandDialog, kCL_File, &dummyType, &dummyHandle, &dummyRect); /* MJS */
				SetControlValue((ControlHandle)dummyHandle, !GetControlValue((ControlHandle)dummyHandle));
			}
		} while (itemHit != kCL_OK && itemHit != kCL_Cancel);

		/* Get control values, even if they did not change */
		GetDialogItem(commandDialog, kCL_Text, &dummyType, &dummyHandle, &dummyRect); /* MJS */
		GetDialogItemText(dummyHandle, prefs.command_line);

		GetDialogItem(commandDialog, kCL_File, &dummyType, &dummyHandle, &dummyRect); /* MJS */
		prefs.output_to_file = GetControlValue((ControlHandle)dummyHandle);

		GetDialogItem(commandDialog, kCL_Video, &dummyType, &dummyHandle, &dummyRect);
		videodriver = GetControlValue((ControlRef)dummyHandle);

		DisposeDialog(commandDialog);

		if (itemHit == kCL_Cancel)
			exit(0);
	}
	
	/* Set pseudo-environment variables for video driver, update prefs */
	switch (videodriver) {
	case VIDEO_ID_DRAWSPROCKET: 
		setenv("SDL_VIDEODRIVER", "DSp", 1);
		str_to_pascal("DSp", prefs.video_driver_name, NULL);
		break;
	case VIDEO_ID_TOOLBOX:
		setenv("SDL_VIDEODRIVER", "toolbox", 1);
		str_to_pascal("toolbox", prefs.video_driver_name, NULL);
		break;
	}

	/* Redirect standard I/O to files */
	if (prefs.output_to_file) {
		freopen(STDOUT_FILE, "w+", stdout);
		freopen(STDERR_FILE, "w+", stderr);
	} else {
		// No! Bad!
		//fclose(stdout);
		//fclose(stderr);
	}

	if (settingsChanged) {
		/* Save the prefs, even if they might not have changed (but probably did) */
		macos_cfg_save(&prefs);
	}

	/* Convert the command line into a C-string */
	str_from_pascal(prefs.command_line, commandLine);

	/* Parse command line into argc,argv */
	nargs = ParseCommandLine(commandLine, NULL) + 1;
	args = mem_alloc((nargs + 1) * (sizeof(*args)));
	ParseCommandLine(commandLine, args + 1);

	{
		/* Get the app name */
		char app_name[256];

		get_app_file_name(app_name);

		/* Cram it into argv[0] */
		args[0] = app_name;

		/* Thus, everything was UTF-8 */
		for (i = 0; i < nargs; i++)
			args[i] = charset_iconv_easy(args[i], CHARSET_SYSTEMSCRIPT, CHARSET_UTF8);
	}

	*pargc = nargs;
	*pargv = args;
}

void macos_sysexit(void)
{
	/* free(args)  <- don't really need this  --paper */
	cleanup_output();
}
