/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

    Feel free to customize this file to suit your needs
*/

#define Cursor AppleCursor
#import <Cocoa/Cocoa.h>
#undef Cursor

@interface SDLMain : NSObject
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
@end
