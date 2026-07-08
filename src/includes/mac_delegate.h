#import <AppKit/NSApplication.h>
#import <UserNotifications/UserNotifications.h>

#include "config.h"
#include "globalshortcuts/globalshortcutsbackend-macos.h"

class PlatformInterface;

@interface AppDelegate : NSObject<NSApplicationDelegate, UNUserNotificationCenterDelegate> {
  PlatformInterface *application_handler_;
  NSMenu *dock_menu_;
  GlobalShortcutsBackendMacOS *shortcut_handler_;
}

- (id) initWithHandler: (PlatformInterface*)handler;

// NSApplicationDelegate
- (BOOL) applicationShouldHandleReopen: (NSApplication*)app hasVisibleWindows:(BOOL)flag;
- (NSMenu*) applicationDockMenu: (NSApplication*)sender;
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification;
- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication*)sender;

// UNUserNotificationCenterDelegate
- (void) userNotificationCenter: (UNUserNotificationCenter*)center
    willPresentNotification: (UNNotification*)notification
    withCompletionHandler: (void (^)(UNNotificationPresentationOptions options))completionHandler;

- (void) setDockMenu: (NSMenu*)menu;
- (GlobalShortcutsBackendMacOS*) shortcut_handler;
- (void) setShortcutHandler: (GlobalShortcutsBackendMacOS*)backend;
@end
