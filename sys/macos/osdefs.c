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
#include "errno.h"
#include "dmoz.h"
#include "charset.h"

#include <Files.h>
#include <Folders.h>
#include <Dialogs.h>

#include <ctype.h>

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

	// These message boxes should really only be taking in ASCII anyway
	str_to_pascal(title, err, NULL);
	str_to_pascal(text, explanation, NULL);

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
		str_to_pascal(normal, mpath, &truncated);
		free(normal);
		
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
		str_to_pascal(normal, ppath, &truncated);
		free(normal);

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
// This assumes that time_t is POSIX-y (i.e. we're under Retro68, or some other
// toolchain that does this). It also assumes the date is in UTC, which is what
// Mac OS Extended uses; Mac OS Standard uses good old local time, but I don't
// really care about that.
#define CONVERT_TIME_TO_POSIX(x) ((int64_t)(x) - INT64_C(2082844800))
				.st_atime = CONVERT_TIME_TO_POSIX(pb.hFileInfo.ioFlMdDat),
				.st_mtime = CONVERT_TIME_TO_POSIX(pb.hFileInfo.ioFlMdDat),
				.st_ctime = CONVERT_TIME_TO_POSIX(pb.hFileInfo.ioFlCrDat),
#undef CONVERT_TIME_TO_POSIX
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

	// 
	if (((unsigned char *)theKeyMap)[6] & 0x80)
		return 1;
	return 0;
}

/* Parse a command line buffer into arguments */
static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	int argc;

	argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		/* Skip leading whitespace */
		while (isspace(*bufp))
			++bufp;

		/* Skip over argument */
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			/* Skip over word */
			while ( *bufp && (*bufp != '"') ) {
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			/* Skip over word */
			while ( *bufp && !isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}

/* Remove the output files if there was no output written */
static void cleanup_output(void)
{
	FILE *file;
	int empty;

	/* Flush the output in case anything is queued */
	fclose(stdout);
	fclose(stderr);

	/* See if the files have any output in them */
	file = fopen(STDOUT_FILE, "rb");
	if ( file ) {
		empty = (fgetc(file) == EOF) ? 1 : 0;
		fclose(file);
		if ( empty ) {
			remove(STDOUT_FILE);
		}
	}
	file = fopen(STDERR_FILE, "rb");
	if ( file ) {
		empty = (fgetc(file) == EOF) ? 1 : 0;
		fclose(file);
		if ( empty ) {
			remove(STDERR_FILE);
		}
	}
}

// XXX this should be adapted for a dmoz.c
static int getCurrentAppName (StrFileName name) {
	ProcessSerialNumber process;
	ProcessInfoRec      process_info;
	FSSpec              process_fsp;

	process.highLongOfPSN = 0;
	process.lowLongOfPSN  = kCurrentProcess;
	process_info.processInfoLength = sizeof (process_info);
	process_info.processName    = NULL;
	process_info.processAppSpec = &process_fsp;

	if ( noErr != GetProcessInformation (&process, &process_info) )
	   return 0;

	memcpy(name, process_fsp.name, process_fsp.name[0] + 1);
	return 1;
}

static int getPrefsFile (FSSpec *prefs_fsp, int create) {
	/* The prefs file name is the application name, possibly truncated, */
	/* plus " Preferences */

	#define  SUFFIX   " Preferences"
	#define  MAX_NAME 19             /* 31 - strlen (SUFFIX) */
	
	short  volume_ref_number;
	long   directory_id;
	StrFileName  prefs_name;
	StrFileName  app_name;

	/* Get Preferences folder - works with Multiple Users */
	if ( noErr != FindFolder ( kOnSystemDisk, kPreferencesFolderType, kDontCreateFolder,
							   &volume_ref_number, &directory_id) )
		exit (-1);

	if ( ! getCurrentAppName (app_name) )
		exit (-1);
	
	/* Truncate if name is too long */
	if (app_name[0] > MAX_NAME )
		app_name[0] = MAX_NAME;
		
	memcpy(prefs_name + 1, app_name + 1, app_name[0]);    
	memcpy(prefs_name + app_name[0] + 1, SUFFIX, strlen (SUFFIX));
	prefs_name[0] = app_name[0] + strlen (SUFFIX);
   
	/* Make the file spec for prefs file */
	if ( noErr != FSMakeFSSpec (volume_ref_number, directory_id, prefs_name, prefs_fsp) ) {
		if ( !create )
			return 0;
		else {
			/* Create the prefs file */
			memcpy(prefs_fsp->name, prefs_name, prefs_name[0] + 1);
			prefs_fsp->parID   = directory_id;
			prefs_fsp->vRefNum = volume_ref_number;
				
			FSpCreateResFile (prefs_fsp, 0x3f3f3f3f, 'pref', 0); // '????' parsed as trigraph
			
			if ( noErr != ResError () )
				return 0;
		}
	 }
	return 1;
}

static int readPrefsResource (PrefsRecord *prefs) {
	
	Handle prefs_handle;
	
	prefs_handle = Get1Resource( 'CLne', 128 );

	if (prefs_handle != NULL) {
		int offset = 0;
//		int j      = 0;
		
		HLock(prefs_handle);
		
		/* Get command line string */	
		memcpy(prefs->command_line, *prefs_handle, (*prefs_handle)[0]+1);

		/* Get video driver name */
		offset += (*prefs_handle)[0] + 1;	
		memcpy(prefs->video_driver_name, *prefs_handle + offset, (*prefs_handle)[offset] + 1);		
		
		/* Get save-to-file option (1 or 0) */
		offset += (*prefs_handle)[offset] + 1;
		prefs->output_to_file = (*prefs_handle)[offset];
		
		ReleaseResource( prefs_handle );
	
		return ResError() == noErr;
	}

	return 0;
}

static int writePrefsResource (PrefsRecord *prefs, short resource_file) {

	Handle prefs_handle;
	
	UseResFile (resource_file);
	
	prefs_handle = Get1Resource ( 'CLne', 128 );
	if (prefs_handle != NULL)
		RemoveResource (prefs_handle);
	
	prefs_handle = NewHandle ( prefs->command_line[0] + prefs->video_driver_name[0] + 4 );
	if (prefs_handle != NULL) {
	
		int offset;
		
		HLock (prefs_handle);
		
		/* Command line text */
		offset = 0;
		memcpy(*prefs_handle, prefs->command_line, prefs->command_line[0] + 1);
		
		/* Video driver name */
		offset += prefs->command_line[0] + 1;
		memcpy(*prefs_handle + offset, prefs->video_driver_name, prefs->video_driver_name[0] + 1);
		
		/* Output-to-file option */
		offset += prefs->video_driver_name[0] + 1;
		*( *((char**)prefs_handle) + offset)     = (char)prefs->output_to_file;
		*( *((char**)prefs_handle) + offset + 1) = 0;
			  
		AddResource   (prefs_handle, 'CLne', 128, "\pCommand Line");
		WriteResource (prefs_handle);
		UpdateResFile (resource_file);
		DisposeHandle (prefs_handle);
		
		return ResError() == noErr;
	}
	
	return 0;
}

static int readPreferences (PrefsRecord *prefs) {

	int    no_error = 1;
	FSSpec prefs_fsp;

	/* Check for prefs file first */
	if ( getPrefsFile (&prefs_fsp, 0) ) {
	
		short  prefs_resource;
		
		prefs_resource = FSpOpenResFile (&prefs_fsp, fsRdPerm);
		if ( prefs_resource == -1 ) /* this shouldn't happen, but... */
			return 0;
	
		UseResFile   (prefs_resource);
		no_error = readPrefsResource (prefs);     
		CloseResFile (prefs_resource);
	}
	
	/* Fall back to application's resource fork (reading only, so this is safe) */
	else {
	
		  no_error = readPrefsResource (prefs);
	 }

	return no_error;
}

static int writePreferences (PrefsRecord *prefs) {
	
	int    no_error = 1;
	FSSpec prefs_fsp;
	
	/* Get prefs file, create if it doesn't exist */
	if ( getPrefsFile (&prefs_fsp, 1) ) {
	
		short  prefs_resource;
		
		prefs_resource = FSpOpenResFile (&prefs_fsp, fsRdWrPerm);
		if (prefs_resource == -1)
			return 0;
		no_error = writePrefsResource (prefs, prefs_resource);
		CloseResFile (prefs_resource);
	}
	
	return no_error;
}

static char   **args = NULL;
static char   *commandLine = NULL;

/* called by main, fixes ~everything~ basically */
void macos_sysinit(int *pargc, char ***pargv)
{
#define DEFAULT_ARGS {'\0'}                /* pascal string for default args */
#define DEFAULT_VIDEO_DRIVER {'\7', 't', 'o', 'o', 'l', 'b', 'o', 'x'} /* pascal string for default video driver name */	
#define DEFAULT_OUTPUT_TO_FILE 1         /* 1 == output to file, 0 == no output */

#define VIDEO_ID_DRAWSPROCKET 1          /* these correspond to popup menu choices */
#define VIDEO_ID_TOOLBOX      2

	PrefsRecord prefs = { DEFAULT_ARGS, DEFAULT_VIDEO_DRIVER, DEFAULT_OUTPUT_TO_FILE }; 

	int     nargs;
	
	StrFileName  appNameText;

	int     videodriver     = VIDEO_ID_TOOLBOX;
	int     settingsChanged = 0;
	
	long	i;

	/* Kyle's SDL command-line dialog code ... */
	InitGraf    (&qd.thePort);
	InitFonts   ();
	InitWindows ();
	InitMenus   ();
	InitDialogs (nil);
	InitCursor ();
	InitContextualMenus();
	FlushEvents(everyEvent,0);
	MaxApplZone ();
	MoreMasters ();
	MoreMasters ();

	 if ( readPreferences (&prefs) ) {
		
		if (memcmp(prefs.video_driver_name+1, "DSp", 3) == 0)
			videodriver = 1;
		else if (memcmp(prefs.video_driver_name+1, "toolbox", 7) == 0)
			videodriver = 2;
	 }
		
	if ( CommandKeyIsDown() ) {

#define kCL_OK		1
#define kCL_Cancel	2
#define kCL_Text	3
#define kCL_File	4
#define kCL_Video   6
	   
		DialogPtr commandDialog;
		short	  dummyType;
		Rect	  dummyRect;
		Handle    dummyHandle;
		short     itemHit;
		
		/* Assume that they will change settings, rather than do exhaustive check */
		settingsChanged = 1;
		
		/* Create dialog and display it */
		commandDialog = GetNewDialog (1000, nil, (WindowPtr)-1);
		SetPort (commandDialog);
		   
		/* Setup controls */
		GetDialogItem   (commandDialog, kCL_File, &dummyType, &dummyHandle, &dummyRect); /* MJS */
		SetControlValue ((ControlHandle)dummyHandle, prefs.output_to_file );

		GetDialogItem     (commandDialog, kCL_Text, &dummyType, &dummyHandle, &dummyRect);
		SetDialogItemText (dummyHandle, prefs.command_line);

		GetDialogItem   (commandDialog, kCL_Video, &dummyType, &dummyHandle, &dummyRect);
		SetControlValue ((ControlRef)dummyHandle, videodriver);

		SetDialogDefaultItem (commandDialog, kCL_OK);
		SetDialogCancelItem  (commandDialog, kCL_Cancel);

		do {
				
			ModalDialog(nil, &itemHit); /* wait for user response */
			
			/* Toggle command-line output checkbox */	
			if ( itemHit == kCL_File ) {
				GetDialogItem(commandDialog, kCL_File, &dummyType, &dummyHandle, &dummyRect); /* MJS */
				SetControlValue((ControlHandle)dummyHandle, !GetControlValue((ControlHandle)dummyHandle) );
			}

		} while (itemHit != kCL_OK && itemHit != kCL_Cancel);

		/* Get control values, even if they did not change */
		GetDialogItem     (commandDialog, kCL_Text, &dummyType, &dummyHandle, &dummyRect); /* MJS */
		GetDialogItemText (dummyHandle, prefs.command_line);

		GetDialogItem (commandDialog, kCL_File, &dummyType, &dummyHandle, &dummyRect); /* MJS */
		prefs.output_to_file = GetControlValue ((ControlHandle)dummyHandle);

		GetDialogItem (commandDialog, kCL_Video, &dummyType, &dummyHandle, &dummyRect);
		videodriver = GetControlValue ((ControlRef)dummyHandle);

		DisposeDialog (commandDialog);

		if (itemHit == kCL_Cancel ) {
			exit (0);
		}
	}
	
	/* Set pseudo-environment variables for video driver, update prefs */
	switch ( videodriver ) {
	   case VIDEO_ID_DRAWSPROCKET: 
		  setenv("SDL_VIDEODRIVER", "DSp", 1);
		  memcpy(prefs.video_driver_name, "\pDSp", 4);
		  break;
	   case VIDEO_ID_TOOLBOX:
		  setenv("SDL_VIDEODRIVER", "toolbox", 1);
		  memcpy(prefs.video_driver_name, "\ptoolbox", 8);
		  break;
	}

	/* Redirect standard I/O to files */
	if ( prefs.output_to_file ) {
		freopen (STDOUT_FILE, "w", stdout);
		freopen (STDERR_FILE, "w", stderr);
	} else {
		fclose (stdout);
		fclose (stderr);
	}

	if (settingsChanged) {
		/* Save the prefs, even if they might not have changed (but probably did) */
		if ( ! writePreferences (&prefs) )
			fprintf (stderr, "WARNING: Could not save preferences!\n");
	}

	appNameText[0] = 0;
	getCurrentAppName (appNameText); /* check for error here ? */

	commandLine = (char*) malloc (appNameText[0] + prefs.command_line[0] + 2);
	if ( commandLine == NULL ) {
	   exit(-1);
	}

	/* Rather than rewrite ParseCommandLine method, let's replace  */
	/* any spaces in application name with underscores,            */
	/* so that the app name is only 1 argument                     */   
	for (i = 1; i < 1+appNameText[0]; i++)
		if ( appNameText[i] == ' ' ) appNameText[i] = '_';

	/* Copy app name & full command text to command-line C-string */      
	memcpy(commandLine, appNameText + 1, appNameText[0]);
	commandLine[appNameText[0]] = ' ';
	memcpy(commandLine + appNameText[0] + 1, prefs.command_line + 1, prefs.command_line[0]);
	commandLine[ appNameText[0] + 1 + prefs.command_line[0] ] = '\0';

	/* Parse C-string into argv and argc */
	nargs = ParseCommandLine (commandLine, NULL);
	args = (char **)malloc((nargs+1)*(sizeof *args));
	if ( args == NULL ) {
		exit(-1);
	}
	ParseCommandLine (commandLine, args);

	*pargc = nargs;
	*pargv = args;
}

void macos_sysexit(void)
{
	free(args);
	free(commandLine);

	cleanup_output();
}
