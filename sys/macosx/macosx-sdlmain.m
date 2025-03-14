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

/* wee....

this is used to do some schism-on-macosx customization
and get access to cocoa stuff

pruned up some here :) -mrsb

 */

/*      SDLMain.m - main entry point for our Cocoa-ized SDL app
	Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
	Non-NIB-Code & other changes: Max Horn <max@quendi.de>

	Feel free to customize this file to suit your needs
*/

#include "headers.h"

#include "it.h"
#include "events.h"
#include "osdefs.h"
#include "mem.h"

#include <crt_externs.h>

#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>

#import <Cocoa/Cocoa.h>

// main.c
extern char *initial_song;

// These constants were renamed in the Sierra SDK
#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
# define NSEventModifierFlagOption NSAlternateKeyMask
# define NSEventModifierFlagControl NSControlKeyMask
# define NSEventModifierFlagCommand NSCommandKeyMask
# define NSEventModifierFlagShift NSShiftKeyMask
# define NSEventModifierFlagFunction NSFunctionKeyMask

# define NSEventTypeKeyDown NSKeyDown
#endif

/* Portions of CPS.h --------------------------------------------------- */

typedef struct CPSProcessSerNum {
	UInt32 lo;
	UInt32 hi;
} CPSProcessSerNum;

extern OSErr CPSGetCurrentProcess(CPSProcessSerNum *psn);
extern OSErr CPSEnableForegroundOperation(CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr CPSSetProcessName(CPSProcessSerNum *psn, char *processname);
extern OSErr CPSSetFrontProcess(CPSProcessSerNum *psn);

/* --------------------------------------------------------------------- */

static int macosx_did_finderlaunch = 0;
static int macosx_launched = 0; // FIXME this sucks

#define KEQ_FN(n) [NSString stringWithFormat:@"%C", (unichar)(NSF##n##FunctionKey)]

@interface NSApplication(OtherMacOSXExtensions)
-(void)setAppleMenu:(NSMenu*)m;
@end

@interface SchismTrackerDelegate : NSObject
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
@end

@interface SchismTracker : NSApplication
@end

@implementation SchismTracker
/* Invoked from the Quit menu item */
- (void)terminate:(id)sender
{
	/* Post a QUIT event */
	schism_event_t event;
	event.type = SCHISM_QUIT;
	events_push_event(&event);
}

- (void)_menu_callback:(id)sender
{
	schism_event_t e;
	NSString *px;
	const char *po;

	/* little hack to ignore keydown events here */
	NSEvent* event = [NSApp currentEvent];
	if (!event)
		return;

	if ([event type] == NSEventTypeKeyDown)
		return;

	px = [sender representedObject];
	po = [px UTF8String];
	if (po) {
		e.type = SCHISM_EVENT_NATIVE_SCRIPT;
		e.script.which = str_dup(po);
		events_push_event(&e);
	}
}

@end /* @implementation SchismTracker */

/* The main class of the application, the application's delegate */
@implementation SchismTrackerDelegate


- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	if (!filename)
		return NO;

	const char *po = [filename UTF8String];
	if (!po)
		return NO;

	// If we already launched, add the event.
	if (macosx_launched) {
		schism_event_t e;
		e.type = SCHISM_EVENT_NATIVE_OPEN;
		e.open.file = str_dup(po);
		events_push_event(&e);
	} else {
		initial_song = str_dup(po);
	}

	return YES;
}

/* other interesting ones:
- (BOOL)application:(NSApplication *)theApplication printFile:(NSString *)filename
- (BOOL)applicationOpenUntitledFile:(NSApplication *)theApplication
*/
/* Set the working directory to the .app's parent directory */
- (void) setupWorkingDirectory:(BOOL)shouldChdir
{
	if (shouldChdir)
	{
		char parentdir[MAXPATHLEN];
		CFURLRef url = CFBundleCopyBundleURL(CFBundleGetMainBundle());
		CFURLRef url2 = CFURLCreateCopyDeletingLastPathComponent(0, url);
		if (CFURLGetFileSystemRepresentation(url2, true, (unsigned char *) parentdir, MAXPATHLEN)) {
			assert ( chdir (parentdir) == 0 );   /* chdir to the binary app's parent */
		}
		CFRelease(url);
		CFRelease(url2);
	}

}

/* Called when the internal event loop has just started running */
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
	/* Set the working directory to the .app's parent directory */
	[self setupWorkingDirectory: (BOOL)macosx_did_finderlaunch];

	macosx_launched = 1;

	/* If we launched from the Finder we have extra arguments that we
	 * don't care about that will trip up the regular main function. */
	exit(schism_main(macosx_did_finderlaunch ? 1 : *_NSGetArgc(), *_NSGetArgv()));
}

@end /* @implementation SchismTrackerDelegate */

/* ------------------------------------------------------- */

static void setApplicationMenu(void)
{
	/* this really needs to be cleaned up  --paper */
	NSMenu *appleMenu;
	NSMenu *otherMenu;
	NSMenuItem *menuItem;

	appleMenu = [[NSMenu alloc] initWithTitle:@""];

	/* Add menu items */
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Help"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(1)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"help"];

	[appleMenu addItem:[NSMenuItem separatorItem]];
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"View Patterns"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(2)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"pattern"];
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Orders/Panning"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(11)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"orders"];
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Variables"
					action:@selector(_menu_callback:)
				 keyEquivalent:KEQ_FN(12)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"variables"];
	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Message Editor"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(9)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagShift];
	[menuItem setRepresentedObject: @"message_edit"];

	[appleMenu addItem:[NSMenuItem separatorItem]];

	[appleMenu addItemWithTitle:@"Hide Schism Tracker" action:@selector(hide:) keyEquivalent:@"h"];

	menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[menuItem setKeyEquivalentModifierMask:(NSEventModifierFlagOption|NSEventModifierFlagCommand)];

	[appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

	[appleMenu addItem:[NSMenuItem separatorItem]];

	[appleMenu addItemWithTitle:@"Quit Schism Tracker" action:@selector(terminate:) keyEquivalent:@"q"];

	/* Put menu into the menubar */
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:appleMenu];
	[[NSApp mainMenu] addItem:menuItem];

	/* File menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"File"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"New..."
				action:@selector(_menu_callback:)
				keyEquivalent:@"n"];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl];
	[menuItem setRepresentedObject: @"new"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Load..."
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(9)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"load"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Save Current"
				action:@selector(_menu_callback:)
				keyEquivalent:@"s"];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl];
	[menuItem setRepresentedObject: @"save"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Save As..."
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(10)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"save_as"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Export..."
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(10)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagShift];
	[menuItem setRepresentedObject: @"export_song"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Message Log"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(11)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagFunction|NSEventModifierFlagControl];
	[menuItem setRepresentedObject: @"logviewer"];
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[[NSApp mainMenu] addItem:menuItem];

	/* Playback menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"Playback"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Show Infopage"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(5)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"info"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play Song"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(5)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl];
	[menuItem setRepresentedObject: @"play"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play Pattern"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(6)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"play_pattern"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play from Order"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(6)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagShift];
	[menuItem setRepresentedObject: @"play_order"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play from Mark/Cursor"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(7)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"play_mark"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Stop"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(8)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"stop"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Calculate Length"
				action:@selector(_menu_callback:)
				keyEquivalent:@"p"];
	[menuItem setKeyEquivalentModifierMask:(NSEventModifierFlagFunction|NSEventModifierFlagControl)];
	[menuItem setRepresentedObject: @"calc_length"];
	menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[[NSApp mainMenu] addItem:menuItem];

	/* Sample menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"Samples"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Sample List"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(3)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"sample_page"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Sample Library"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(3)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagShift];
	[menuItem setRepresentedObject: @"sample_library"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Reload Soundcard"
				action:@selector(_menu_callback:)
				keyEquivalent:@"g"];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl];
	[menuItem setRepresentedObject: @"init_sound"];
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[[NSApp mainMenu] addItem:menuItem];

	/* Instrument menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"Instruments"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Instrument List"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(4)];
	[menuItem setKeyEquivalentModifierMask:0];
	[menuItem setRepresentedObject: @"inst_page"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Instrument Library"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(4)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagShift];
	[menuItem setRepresentedObject: @"inst_library"];
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[[NSApp mainMenu] addItem:menuItem];

	/* Settings menu */
	otherMenu = [[NSMenu alloc] initWithTitle:@"Settings"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Preferences"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(5)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagShift];
	[menuItem setRepresentedObject: @"preferences"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"MIDI Configuration"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(1)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagShift];
	[menuItem setRepresentedObject: @"midi_config"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Palette Editor"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(12)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl];
	[menuItem setRepresentedObject: @"palette_page"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Font Editor"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(12)];
		[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagShift];
	[menuItem setRepresentedObject: @"font_editor"];
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"System Configuration"
				action:@selector(_menu_callback:)
				keyEquivalent:KEQ_FN(1)];
	[menuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl];
	[menuItem setRepresentedObject: @"system_config"];

	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Toggle Fullscreen"
				action:@selector(_menu_callback:)
				keyEquivalent:@"\r"];
	[menuItem setKeyEquivalentModifierMask:(NSEventModifierFlagControl|NSEventModifierFlagOption)];
	[menuItem setRepresentedObject: @"fullscreen"];
	menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:otherMenu];
	[[NSApp mainMenu] addItem:menuItem];

	/* Tell the application object that this is now the application menu */
	[NSApp setAppleMenu:appleMenu];

	/* Finally give up our references to the objects */
	[appleMenu release];
	[menuItem release];
}

/* Create a window menu */
static void setupWindowMenu(void)
{
	NSMenu      *windowMenu;
	NSMenuItem  *windowMenuItem;

	windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

	/* "Minimize" item */
	{
		NSMenuItem  *menuItem;

		menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
		[windowMenu addItem:menuItem];

		/* DON'T WORRY! I'M GONNA CUSHION OUR FALL WITH THIS WATER BUCKET! */
		[menuItem release];
	}

	/* Put menu into the menubar */
	windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent: @""];
	[windowMenuItem setSubmenu:windowMenu];
	[[NSApp mainMenu] addItem:windowMenuItem];

	/* Tell the application object that this is now the window menu */
	[NSApp setWindowsMenu:windowMenu];

	/* Finally give up our references to the objects */
	[windowMenu release];
	[windowMenuItem release];
}

#ifdef main
#  undef main
#endif

/* Main entry point to executable - should *not* be SDL_main! */
int main (int argc, char **argv)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	/* Copy the arguments into a global variable -- this
	 * is passed if we are launched by double-clicking */
	macosx_did_finderlaunch = (argc >= 2 && strncmp (argv[1], "-psn", 4) == 0);

	{
		CPSProcessSerNum psn;

		/* Tell the dock about us */
		if (!CPSGetCurrentProcess(&psn)) {
			if (!macosx_did_finderlaunch)
				CPSSetProcessName(&psn, "Schism Tracker");

			if (!CPSEnableForegroundOperation(&psn, 0x03, 0x3C, 0x2C, 0x1103))
				if (!CPSSetFrontProcess(&psn))
					[SchismTracker sharedApplication];
		}
	}

	/* Ensure the application object is initialised */
	[SchismTracker sharedApplication];

	/* Set up the menubar */
	[NSApp setMainMenu:[[NSMenu alloc] init]];
	setApplicationMenu();
	setupWindowMenu();

	/* Create the app delegate */
	SchismTrackerDelegate *delegate = [[SchismTrackerDelegate alloc] init];
	[NSApp setDelegate: (id)delegate]; /* stupid cast to silence compiler */

	/* Start the main event loop */
	[NSApp run];

	[delegate release];
	[pool release];

	return 0;
}

/* ---------------------------------------------------------------- */

int macosx_get_key_repeat(int *pdelay, int *prate)
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	if (!defaults)
		return 0;

	int delay = [defaults integerForKey:@"InitialKeyRepeat"];
	int rate = [defaults integerForKey:@"KeyRepeat"];

	// apparently these will never be zero.
	if (!delay || delay < 0 || !rate || rate < 0)
		return 0;

	// According to this Apple Discussions thread, these
	// values are milliseconds divided by 15:
	//
	// https://discussions.apple.com/thread/1316947
	*pdelay = delay * 15;
	*prate = rate * 15;

	return 1;
}

char *macosx_get_application_support_dir(void)
{
	NSArray* strings = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, true);
	if ([strings count] < 1)
		return NULL;

	NSString *path = [strings objectAtIndex: 0];
	if (!path)
		return NULL;

	return str_dup([path UTF8String]);
}
