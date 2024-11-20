#import <AppKit/NSApplication.h>

#include "config.h"
#include "globalshortcuts/globalshortcutsbackend-macos.h"

class PlatformInterface;
//@class SPMediaKeyTap;

@interface AppDelegate : NSObject<NSApplicationDelegate, NSUserNotificationCenterDelegate> {
  PlatformInterface *application_handler_;
  NSMenu *dock_menu_;
  GlobalShortcutsBackendMacOS *shortcut_handler_;
  //SPMediaKeyTap *key_tap_;

}

- (id) initWithHandler: (PlatformInterface*)handler;

// NSApplicationDelegate
- (BOOL) applicationShouldHandleReopen: (NSApplication*)app hasVisibleWindows:(BOOL)flag;
- (NSMenu*) applicationDockMenu: (NSApplication*)sender;
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification;
- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication*)sender;

// NSUserNotificationCenterDelegate
- (BOOL) userNotificationCenter: (id)center
    shouldPresentNotification: (id)notification;

- (void) setDockMenu: (NSMenu*)menu;
- (GlobalShortcutsBackendMacOS*) shortcut_handler;
- (void) setShortcutHandler: (GlobalShortcutsBackendMacOS*)backend;
//- (void) mediaKeyTap: (SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event;
@end
