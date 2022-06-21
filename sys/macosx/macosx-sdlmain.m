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

#define Cursor AppleCursor
#import <Cocoa/Cocoa.h>
#undef Cursor

@interface SDLMain : NSObject
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
@end

#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>

/* Portions of CPS.h */
typedef struct CPSProcessSerNum
{
        UInt32          lo;
        UInt32          hi;
} CPSProcessSerNum;

extern OSErr    CPSGetCurrentProcess( CPSProcessSerNum *psn);
extern OSErr    CPSEnableForegroundOperation( CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr    CPSSetProcessName ( CPSProcessSerNum *psn, char *processname);
extern OSErr    CPSSetFrontProcess( CPSProcessSerNum *psn);

static int    gArgc;
static char  **gArgv;
static BOOL   gFinderLaunch;
int macosx_did_finderlaunch;

#define KEQ_FN(n) [NSString stringWithFormat:@"%C", NSF##n##FunctionKey]

@interface SDLApplication : NSApplication
@end

@interface NSApplication(OtherMacOSXExtensions)
-(void)setAppleMenu:(NSMenu*)m;
@end

@implementation SDLApplication
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
        SDL_Event e;
        NSString *px;
        const char *po;

        px = [sender representedObject];
        po = [px UTF8String];
        if (po) {
                e.type = SCHISM_EVENT_NATIVE;
                e.user.code = SCHISM_EVENT_NATIVE_SCRIPT;
                e.user.data1 = strdup(po);
                SDL_PushEvent(&e);
        }
}


@end

/* The main class of the application, the application's delegate */
@implementation SDLMain

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
                a document, then Main still hasn't really started yet.
                */
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

static void setApplicationMenu(void)
{
        /* warning: this code is very odd */
        NSMenu *appleMenu;
        NSMenu *otherMenu;
        NSMenuItem *menuItem;

        appleMenu = [[NSMenu alloc] initWithTitle:@""];

        /* Add menu items */
        [appleMenu addItemWithTitle:@"About Schism Tracker"
                        action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

        [appleMenu addItem:[NSMenuItem separatorItem]];

        /* other schism items */
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
                                 keyEquivalent:[NSString stringWithFormat:@"%C", NSF12FunctionKey]];
        [menuItem setKeyEquivalentModifierMask:0];
        [menuItem setRepresentedObject: @"variables"];
        menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Message Editor"
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(9)];
        [menuItem setKeyEquivalentModifierMask:NSShiftKeyMask];
        [menuItem setRepresentedObject: @"message_edit"];

        [appleMenu addItem:[NSMenuItem separatorItem]];

        [appleMenu addItemWithTitle:@"Hide Schism Tracker" action:@selector(hide:) keyEquivalent:@"h"];

        menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        [menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

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
        [menuItem setKeyEquivalentModifierMask:NSControlKeyMask];
        [menuItem setRepresentedObject: @"new"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Load..."
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(9)];
        [menuItem setKeyEquivalentModifierMask:0];
        [menuItem setRepresentedObject: @"load"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Save Current"
                                action:@selector(_menu_callback:)
                                keyEquivalent:@"s"];
        [menuItem setKeyEquivalentModifierMask:NSControlKeyMask];
        [menuItem setRepresentedObject: @"save"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Save As..."
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(10)];
        [menuItem setKeyEquivalentModifierMask:0];
        [menuItem setRepresentedObject: @"save_as"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Export..."
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(10)];
        [menuItem setKeyEquivalentModifierMask:NSShiftKeyMask];
        [menuItem setRepresentedObject: @"export_song"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Message Log"
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(11)];
        [menuItem setKeyEquivalentModifierMask:NSFunctionKeyMask|NSControlKeyMask];
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
        [menuItem setKeyEquivalentModifierMask:NSControlKeyMask];
        [menuItem setRepresentedObject: @"play"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play Pattern"
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(6)];
        [menuItem setKeyEquivalentModifierMask:0];
        [menuItem setRepresentedObject: @"play_pattern"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Play from Order"
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(6)];
        [menuItem setKeyEquivalentModifierMask:NSShiftKeyMask];
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
        [menuItem setKeyEquivalentModifierMask:(NSFunctionKeyMask|NSControlKeyMask)];
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
        [menuItem setKeyEquivalentModifierMask:NSShiftKeyMask];
        [menuItem setRepresentedObject: @"sample_library"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Reload Soundcard"
                                action:@selector(_menu_callback:)
                                keyEquivalent:@"g"];
        [menuItem setKeyEquivalentModifierMask:NSControlKeyMask];
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
        [menuItem setKeyEquivalentModifierMask:NSShiftKeyMask];
        [menuItem setRepresentedObject: @"inst_library"];
        menuItem = (NSMenuItem*)[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:otherMenu];
        [[NSApp mainMenu] addItem:menuItem];

        /* Settings menu */
        otherMenu = [[NSMenu alloc] initWithTitle:@"Settings"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Preferences"
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(5)];
        [menuItem setKeyEquivalentModifierMask:NSShiftKeyMask];
        [menuItem setRepresentedObject: @"preferences"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"MIDI Configuration"
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(1)];
        [menuItem setKeyEquivalentModifierMask:NSShiftKeyMask];
        [menuItem setRepresentedObject: @"midi_config"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Palette Editor"
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(12)];
        [menuItem setKeyEquivalentModifierMask:NSControlKeyMask];
        [menuItem setRepresentedObject: @"palette_page"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Font Editor"
                                action:@selector(_menu_callback:)
                                keyEquivalent:@""];
        [menuItem setRepresentedObject: @"font_editor"];
        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"System Configuration"
                                action:@selector(_menu_callback:)
                                keyEquivalent:KEQ_FN(1)];
        [menuItem setKeyEquivalentModifierMask:NSControlKeyMask];
        [menuItem setRepresentedObject: @"system_config"];

        menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Toggle Fullscreen"
                                action:@selector(_menu_callback:)
                                keyEquivalent:@"\r"];
        [menuItem setKeyEquivalentModifierMask:(NSControlKeyMask|NSAlternateKeyMask)];
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
        NSMenuItem  *menuItem;

        windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

        /* "Minimize" item */
        menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
        [windowMenu addItem:menuItem];
        [menuItem release];

        /* Put menu into the menubar */
        windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
        [windowMenuItem setSubmenu:windowMenu];
        [[NSApp mainMenu] addItem:windowMenuItem];

        /* Tell the application object that this is now the window menu */
        [NSApp setWindowsMenu:windowMenu];

        /* Finally give up our references to the objects */
        [windowMenu release];
        [windowMenuItem release];
}

/* Replacement for NSApplicationMain */
static void CustomApplicationMain (int argc, char **argv)
{
        NSAutoreleasePool       *pool = [[NSAutoreleasePool alloc] init];
        SDLMain                         *sdlMain;
        CPSProcessSerNum PSN;

        /* Ensure the application object is initialised */
        [SDLApplication sharedApplication];

        /* Tell the dock about us */
        if (!CPSGetCurrentProcess(&PSN)) {
                if (!macosx_did_finderlaunch) {
                        CPSSetProcessName(&PSN,"Schism Tracker");
                }
                if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
                        if (!CPSSetFrontProcess(&PSN))
                                [SDLApplication sharedApplication];
        }

        /* Set up the menubar */
        [NSApp setMainMenu:[[NSMenu alloc] init]];
        setApplicationMenu();
        setupWindowMenu();

        /* Create SDLMain and make it the app delegate */
        sdlMain = [[SDLMain alloc] init];
        [NSApp setDelegate:sdlMain];

        /* Start the main event loop */
        [NSApp run];

        [sdlMain release];
        [pool release];
}

/* Called when the internal event loop has just started running */
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
        int status;

        /* Set the working directory to the .app's parent directory */
        [self setupWorkingDirectory:gFinderLaunch];

        /* Hand off to main application code */
        status = SDL_Init (gArgv);

        /* We're done, thank you for playing */
        exit(status);
}
@end


@implementation NSString (ReplaceSubString)

- (NSString *)stringByReplacingRange:(NSRange)aRange with:(NSString *)aString
{
        unsigned int bufferSize;
        unsigned int selfLen = [self length];
        unsigned int aStringLen = [aString length];
        unichar *buffer;
        NSRange localRange;
        NSString *result;

        bufferSize = selfLen + aStringLen - aRange.length;
        buffer = NSAllocateMemoryPages(bufferSize*sizeof(unichar));

        /* Get first part into buffer */
        localRange.location = 0;
        localRange.length = aRange.location;
        [self getCharacters:buffer range:localRange];

        /* Get middle part into buffer */
        localRange.location = 0;
        localRange.length = aStringLen;
        [aString getCharacters:(buffer+aRange.location) range:localRange];

        /* Get last part into buffer */
        localRange.location = aRange.location + aRange.length;
        localRange.length = selfLen - localRange.location;
        [self getCharacters:(buffer+aRange.location+aStringLen) range:localRange];

        /* Build output string */
        result = [NSString stringWithCharacters:buffer length:bufferSize];

        NSDeallocateMemoryPages(buffer, bufferSize);

        return result;
}

@end



#ifdef main
#  undef main
#endif


/* Main entry point to executable - should *not* be SDL_main! */
/* JK LOL... SDL2 */
int SDL_main (int argc, char **argv)
{
        /* Copy the arguments into a global variable */
        /* This is passed if we are launched by double-clicking */
        if ( argc >= 2 && strncmp (argv[1], "-psn", 4) == 0 ) {
                gArgc = 1;
                gFinderLaunch = YES;
                macosx_did_finderlaunch = 1;
        } else {
                gArgc = argc;
                gFinderLaunch = NO;
                macosx_did_finderlaunch = 0;
        }
        gArgv = argv;

        CustomApplicationMain (argc, argv);

        return 0;
}

/* these routines provide clipboard encapsulation */
const char *macosx_clippy_get(void)
{
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        NSString *type = [pb availableTypeFromArray:[NSArray
                                        arrayWithObject:NSStringPboardType]];
        NSString *contents;
        const char *po;

        if (type == nil) return "";

        contents = [pb stringForType:type];
        if (contents == nil) return "";
        po = [contents UTF8String];
        if (!po) return "";
        return po;
}
void macosx_clippy_put(const char *buf)
{
        NSString *contents = [NSString stringWithUTF8String:buf];
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
        [pb setString:contents forType:NSStringPboardType];
}
int key_scancode_lookup(int k, int def)
{
        switch (k & 127) {
        case 0x32: /* QZ_BACKQUOTE */ return SDLK_BACKQUOTE;
        case 0x12: /* QZ_1 */ return SDLK_1;
        case 0x13: /* QZ_2 */ return SDLK_2;
        case 0x14: /* QZ_3 */ return SDLK_3;
        case 0x15: /* QZ_4 */ return SDLK_4;
        case 0x17: /* QZ_5 */ return SDLK_5;
        case 0x16: /* QZ_6 */ return SDLK_6;
        case 0x1A: /* QZ_7 */ return SDLK_7;
        case 0x1C: /* QZ_8 */ return SDLK_8;
        case 0x19: /* QZ_9 */ return SDLK_9;
        case 0x1D: /* QZ_0 */ return SDLK_0;
        case 0x1B: /* QZ_MINUS */ return SDLK_MINUS;
        case 0x18: /* QZ_EQUALS */ return SDLK_EQUALS;
        case 0x0C: /* QZ_q */ return SDLK_q;
        case 0x0D: /* QZ_w */ return SDLK_w;
        case 0x0E: /* QZ_e */ return SDLK_e;
        case 0x0F: /* QZ_r */ return SDLK_r;
        case 0x11: /* QZ_t */ return SDLK_t;
        case 0x10: /* QZ_y */ return SDLK_y;
        case 0x20: /* QZ_u */ return SDLK_u;
        case 0x22: /* QZ_i */ return SDLK_i;
        case 0x1F: /* QZ_o */ return SDLK_o;
        case 0x23: /* QZ_p */ return SDLK_p;
        case 0x21: /* QZ_[ */ return SDLK_LEFTBRACKET;
        case 0x1E: /* QZ_] */ return SDLK_RIGHTBRACKET;
        case 0x2A: /* QZ_backslash */ return SDLK_BACKSLASH;
        case 0x00: /* QZ_a */ return SDLK_a;
        case 0x01: /* QZ_s */ return SDLK_s;
        case 0x02: /* QZ_d */ return SDLK_d;
        case 0x03: /* QZ_f */ return SDLK_f;
        case 0x05: /* QZ_g */ return SDLK_g;
        case 0x04: /* QZ_h */ return SDLK_h;
        case 0x26: /* QZ_j */ return SDLK_j;
        case 0x28: /* QZ_k */ return SDLK_k;
        case 0x25: /* QZ_l */ return SDLK_l;
        case 0x29: /* QZ_; */ return SDLK_SEMICOLON;
        case 0x27: /* QZ_quote */ return SDLK_QUOTE;
        case 0x06: /* QZ_z */ return SDLK_z;
        case 0x07: /* QZ_x */ return SDLK_x;
        case 0x08: /* QZ_c */ return SDLK_c;
        case 0x09: /* QZ_v */ return SDLK_v;
        case 0x0B: /* QZ_b */ return SDLK_b;
        case 0x2D: /* QZ_n */ return SDLK_n;
        case 0x2E: /* QZ_m */ return SDLK_m;
        case 0x2B: /* QZ_, */ return SDLK_COMMA;
        case 0x2F: /* QZ_. */ return SDLK_PERIOD;
        case 0x2C: /* QZ_slash */ return SDLK_SLASH;
        case 0x31: /* QZ_space */ return SDLK_SPACE;
        default: return def;
        };
}
