/* wee....

this is used to do some schism-on-macosx customization
and get access to cocoa stuff

pruned up some here :) -mrsb

 */

/*	SDLMain.m - main entry point for our Cocoa-ized SDL app
	Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
	Non-NIB-Code & other changes: Max Horn <max@quendi.de>

	Feel free to customize this file to suit your needs
*/

extern char *initial_song;

#include <SDL.h> /* necessary here */
#include "event.h"

#import "macosx-sdlmain.h"

#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>

/* Portions of CPS.h */
typedef struct CPSProcessSerNum
{
	UInt32		lo;
	UInt32		hi;
} CPSProcessSerNum;

extern OSErr	CPSGetCurrentProcess( CPSProcessSerNum *psn);
extern OSErr 	CPSEnableForegroundOperation( CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr	CPSSetFrontProcess( CPSProcessSerNum *psn);

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
#if 0 /* This doesn't seem to be necessary at all, and it breaks the normal
	 command line handling (like being able to drag-and-drop directories
	 to browse them) */
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
#endif
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
	menuItem = (NSMenuItem*)[otherMenu addItemWithTitle:@"Toggle Fullscreen"
				action:@selector(_menu_callback:)
				keyEquivalent:@"\r"];
	[menuItem setKeyEquivalentModifierMask:(NSControlKeyMask|NSCommandKeyMask)];
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
static void CustomApplicationMain (argc, argv)
{
	NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
	SDLMain				*sdlMain;
	CPSProcessSerNum PSN;

	/* Ensure the application object is initialised */
	[SDLApplication sharedApplication];
	
	/* Tell the dock about us */
	if (!CPSGetCurrentProcess(&PSN)) {
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
	status = SDL_main (gArgc, gArgv);

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
int main (int argc, char **argv)
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
// ktt appears to be 1/60th of a second?
unsigned key_repeat_rate(void)
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	int ktt = [defaults integerForKey:@"KeyRepeat"];
	if (!ktt || ktt < 0) ktt = 4; // eh?
	ktt = (ktt * 1000) / 60;
	return (unsigned)ktt;
}
unsigned key_repeat_delay(void)
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	int ktt = [defaults integerForKey:@"InitialKeyRepeat"];
	if (!ktt || ktt < 0) ktt = 35;
	ktt = (ktt * 1000) / 60;
	return (unsigned)ktt;
}
