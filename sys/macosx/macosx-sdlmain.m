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

extern char *initial_song;

#include <SDL.h> /* necessary here */
#include "event.h"
#include "osdefs.h"
#include "keybinds.h"

#include <crt_externs.h>

/* Old versions of SDL define "vector" as a GCC
 * builtin; this causes importing Cocoa to fail,
 * as one struct Cocoa needs is named "vector".
 *
 * This only happens on PowerPC. */
#ifdef vector
#undef vector
#endif

#import <Cocoa/Cocoa.h>

@interface SchismTrackerDelegate : NSObject
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
@end

#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>

/* Portions of CPS.h */
typedef struct CPSProcessSerNum
{
	UInt32 lo;
	UInt32 hi;
} CPSProcessSerNum;

extern OSErr CPSGetCurrentProcess(CPSProcessSerNum *psn);
extern OSErr CPSEnableForegroundOperation(CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr CPSSetProcessName(CPSProcessSerNum *psn, char *processname);
extern OSErr CPSSetFrontProcess(CPSProcessSerNum *psn);

static int macosx_did_finderlaunch;

@interface NSApplication(OtherMacOSXExtensions)
-(void)setAppleMenu:(NSMenu*)m;
@end

@interface SchismTracker : NSApplication
@end

@implementation SchismTracker
/* Invoked from the Quit menu item */
- (void)terminate:(id)sender
{
	/* Post a SDL_QUIT event */
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

- (void)_menu_callback:(id)sender
{
	/* ignore keydown events here */
	NSEvent* event = [NSApp currentEvent];
	if (!event || [event type] == NSKeyDown)
		return;

	struct keybinds_menu_item *i = [sender representedObject];

	SDL_Event e;
	keybinds_menu_item_pressed(i, &e);
	SDL_PushEvent(&e);
}

@end

/* The main class of the application, the application's delegate */
@implementation SchismTrackerDelegate

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	SDL_Event e;
	const char *po;

	if (!filename) return NO;

	po = [filename UTF8String];
	if (po) {
		e.type = SCHISM_EVENT_NATIVE;
		e.user.code = SCHISM_EVENT_NATIVE_OPEN;
		e.user.data1 = strdup(po);
		/* if we started as a result of a doubleclick on
		 * a document, then Main still hasn't really started yet. */
		initial_song = strdup(po);
		SDL_PushEvent(&e);
		return YES;
	} else {
		return NO;
	}
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

	exit(SDL_main(*_NSGetArgc(), *_NSGetArgv()));
}

@end /* @implementation SchismTracker */

/* ------------------------------------------------------- */

/* building the menus */

static NSString *get_key_equivalent(keybind_bind_t *bind)
{
	const keybind_shortcut_t* shortcut = &bind->shortcuts[0];
	const SDL_Keycode kc = (shortcut->keycode != SDLK_UNKNOWN)
		? (shortcut->keycode)
		: (shortcut->scancode != SDL_SCANCODE_UNKNOWN)
			? SDL_GetKeyFromScancode(shortcut->scancode)
			: SDLK_UNKNOWN;

	if (kc == SDLK_UNKNOWN)
		return @"";

	switch (kc) {
#define KEQ_FN(n) [NSString stringWithFormat:@"%C", (unichar)(NSF##n##FunctionKey)]
	case SDLK_F1:  return KEQ_FN(1);
	case SDLK_F2:  return KEQ_FN(2);
	case SDLK_F3:  return KEQ_FN(3);
	case SDLK_F4:  return KEQ_FN(4);
	case SDLK_F5:  return KEQ_FN(5);
	case SDLK_F6:  return KEQ_FN(6);
	case SDLK_F7:  return KEQ_FN(7);
	case SDLK_F8:  return KEQ_FN(8);
	case SDLK_F9:  return KEQ_FN(9);
	case SDLK_F10: return KEQ_FN(10);
	case SDLK_F11: return KEQ_FN(11);
	case SDLK_F12: return KEQ_FN(12);
#undef KEQ_FN
	default:
		/* unichar is 16-bit, can't hold more than that */
		if (kc & 0x80000000 || kc > 0xFFFF)
			return @"";

		return [NSString stringWithFormat:@"%C", (unichar)kc];
	}
}

static int get_key_equivalent_modifier(keybind_bind_t *bind)
{
	const enum keybind_modifier mod = bind->shortcuts[0].modifier;
	int modifiers = 0;

	// TODO L and R variants

	if (mod & KEYBIND_MOD_CTRL)
		modifiers |= NSControlKeyMask;

	if (mod & KEYBIND_MOD_SHIFT)
		modifiers |= NSShiftKeyMask;

	if (mod & KEYBIND_MOD_ALT)
		modifiers |= NSAlternateKeyMask;

	return modifiers;
}

static void setApplicationMenu(NSMenu *menu)
{
	for (const struct keybinds_menu *m = keybinds_menus; m->type != KEYBINDS_MENU_NULL; m++) {
		if (m->type != KEYBINDS_MENU_REGULAR && m->type != KEYBINDS_MENU_APPLE)
			continue;

		NSMenu *submenu = [[NSMenu alloc] init];

		for (const struct keybinds_menu_item *i = m->items; i->type != KEYBINDS_MENU_ITEM_NULL; i++) {
			if (i->no_osx)
				continue;

			switch (i->type) {
			case KEYBINDS_MENU_ITEM_REGULAR: {
				/* get the name, but remove the & symbols for alt crap on other systems */
				NSString *name = [NSString stringWithUtf8String: i->info.regular.name];
				name = [name stringByReplacingOccurrencesOfString:@"&" withString:@""];

				/* Add menu item */
				NSMenuItem *item = (NSMenuItem*)[submenu addItemWithTitle:name
							action:@selector(_menu_callback:)
							keyEquivalent:get_key_equivalent(&global_keybinds_list.global.help)];
				[item setKeyEquivalentModifierMask:get_key_equivalent_modifier(&global_keybinds_list.global.help)];
				[item setRepresentedObject: i];
				break;
			}
			case KEYBINDS_MENU_ITEM_SEPARATOR:
				[submenu addItem:[NSMenuItem separatorItem]];
				break;
			default:
				break;
			}
		}

		if (m->type == KEYBINDS_MENU_APPLE) {
			[submenu addItemWithTitle:@"Hide Schism Tracker" action:@selector(hide:) keyEquivalent:@"h"];

			NSMenuItem *item = (NSMenuItem *)[submenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
			[item setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

			[submenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

			[submenu addItem:[NSMenuItem separatorItem]];

			[submenu addItemWithTitle:@"Quit Schism Tracker" action:@selector(terminate:) keyEquivalent:@"q"];
		}

		NSString *name = [NSString stringWithUtf8String: m->info.regular.name];
		name = [name stringByReplacingOccurrencesOfString:@"&" withString:@""];

		/* put menu into the menubar */
		NSMenuItem *item = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:name action:nil keyEquivalent:@""];
		[item setSubmenu:submenu];
		[menu addItem:item];

		[submenu release];
		[item release];
	}
}

/* Create a window menu */
static void setupWindowMenu(NSMenu *menu)
{
	NSMenu      *windowMenu;
	NSMenuItem  *windowMenuItem;
	NSMenuItem  *menuItem;

	windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

	/* "Minimize" item */
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
	[windowMenu addItem:menuItem];
	[menuItem release];

	/* Put menu into the menubar */
	windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent: @""];
	[windowMenuItem setSubmenu:windowMenu];
	[menu addItem:windowMenuItem];

	/* Tell the application object that this is now the window menu */
	[NSApp setWindowsMenu:windowMenu];

	/* Finally give up our references to the objects */
	[windowMenu release];
	[windowMenuItem release];
}

#ifdef main
#  undef main
#endif

void macosx_create_menu(void)
{
	NSMenu *menu = [[NSMenu alloc] init];
	setApplicationMenu(menu);
	setupWindowMenu(menu);
	[NSApp setMainMenu: menu];
}

/* Main entry point to executable - should *not* be SDL_main! */
int main(int argc, char **argv)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	/* Copy the arguments into a global variable */
	/* This is passed if we are launched by double-clicking */
	macosx_did_finderlaunch = (argc >= 2 && strncmp (argv[1], "-psn", 4) == 0);

	{
		CPSProcessSerNum PSN;
		/* Tell the dock about us */
		if (!CPSGetCurrentProcess(&PSN)) {
			if (!macosx_did_finderlaunch)
				CPSSetProcessName(&PSN,"Schism Tracker");

			if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
				if (!CPSSetFrontProcess(&PSN))
					[SchismTracker sharedApplication];
		}
	}

	/* Ensure the application object is initialised */
	[SchismTracker sharedApplication];

	/* Create the app delegate */
	SchismTrackerDelegate *delegate = [[SchismTrackerDelegate alloc] init];
	[NSApp setDelegate: delegate];

	/* Start the main event loop */
	[NSApp run];

	[delegate release];
	[pool release];

	return 0;
}
