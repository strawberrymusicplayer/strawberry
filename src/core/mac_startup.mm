/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSNibDeclarations.h>
#import <AppKit/NSViewController.h>

#import <Foundation/NSBundle.h>
#import <Foundation/NSError.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSThread.h>
#import <Foundation/NSTimer.h>
#import <Foundation/NSURL.h>

#import <IOKit/hidsystem/ev_keymap.h>

#import <Kernel/AvailabilityMacros.h>

#import <QuartzCore/CALayer.h>

//#import <SPMediaKeyTap.h>

#include "config.h"

#include <QApplication>
#include <QCoreApplication>
#include <QWidget>
#include <QDir>
#include <QEvent>
#include <QFile>

#include "includes/mac_delegate.h"
#include "includes/scoped_cftyperef.h"
#include "core/scoped_nsautorelease_pool.h"
#include "core/logging.h"
#include "core/platforminterface.h"
#include "mac_startup.h"
#include "globalshortcuts/globalshortcutsmanager.h"
#include "globalshortcuts/globalshortcutsbackend-macos.h"

QDebug operator<<(QDebug dbg, NSObject *object) {

  const QString ns_format = QString::fromUtf8([[NSString stringWithFormat:@"%@", object] UTF8String]);
  dbg.nospace() << ns_format;
  return dbg.space();

}

// Capture global media keys on Mac (Cocoa only!)
// See: http://www.rogueamoeba.com/utm/2007/09/29/apple-keyboard-media-key-event-handling/

@interface MacApplication : NSApplication {

  PlatformInterface *application_handler_;
  AppDelegate *delegate_;
  // shortcut_handler_ only used to temporarily save it AppDelegate does all the heavy-shortcut-lifting
  GlobalShortcutsBackendMacOS *shortcut_handler_;

}

- (GlobalShortcutsBackendMacOS*)shortcut_handler;
- (void)SetShortcutHandler:(GlobalShortcutsBackendMacOS*)handler;

- (PlatformInterface*)application_handler;
- (void)SetApplicationHandler:(PlatformInterface*)handler;

@end

@implementation AppDelegate

- (id)init {

  if ((self = [super init])) {
    application_handler_ = nil;
    shortcut_handler_ = nil;
    dock_menu_ = nil;
  }
  return self;

}

- (id)initWithHandler:(PlatformInterface*)handler {

  application_handler_ = handler;

  return self;

}

- (BOOL) applicationShouldHandleReopen: (NSApplication*)app hasVisibleWindows:(BOOL)flag {

  Q_UNUSED(app);
  Q_UNUSED(flag);

  if (application_handler_) {
    application_handler_->Activate();
  }
  return YES;

}

- (void)setDockMenu:(NSMenu*)menu {
  dock_menu_ = menu;
}

- (NSMenu*)applicationDockMenu:(NSApplication*)sender {
  Q_UNUSED(sender);
  return dock_menu_;
}

- (void)setShortcutHandler:(GlobalShortcutsBackendMacOS*)backend {
  shortcut_handler_ = backend;
}

- (GlobalShortcutsBackendMacOS*)shortcut_handler {
  return shortcut_handler_;
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {

  Q_UNUSED(aNotification);

  [[NSAppleEventManager sharedAppleEventManager] setEventHandler:self andSelector:@selector(handleURLEvent:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];

  // key_tap_ = [[SPMediaKeyTap alloc] initWithDelegate:self];
  // if ([SPMediaKeyTap usesGlobalMediaKeyTap]) {
  //   if ([key_tap_ startWatchingMediaKeys]) {
  //       qLog(Debug) << "Media key monitoring started";
  //   }
  //   else {
  //       qLog(Warning) << "Failed to start media key monitoring";
  //   }
  // }
  // else {
  //   qLog(Warning) << "Media key monitoring disabled";
  // }

}

- (BOOL)application:(NSApplication*)app openFile:(NSString*)filename {

  Q_UNUSED(app);

  qLog(Debug) << "Wants to open:" << [filename UTF8String];

  if (application_handler_->LoadUrl(QString::fromUtf8([filename UTF8String]))) {
    return YES;
  }

  return NO;

}

- (void)application:(NSApplication*)app openFiles:(NSArray*)filenames {

  qLog(Debug) << "Wants to open:" << filenames;
  [filenames enumerateObjectsUsingBlock:^(id object, NSUInteger, BOOL*) {
    [self application:app openFile:reinterpret_cast<NSString*>(object)];
  }];

}

- (void)handleURLEvent:(NSAppleEventDescriptor*)theEvent withReplyEvent:(NSAppleEventDescriptor*)replyEvent {

  #pragma unused(replyEvent)

  NSString *url = [[theEvent paramDescriptorForKeyword:keyDirectObject] stringValue];
  application_handler_->LoadUrl(QString::fromNSString(url));

}

// - (void) mediaKeyTap: (SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event {
//   #pragma unused(keyTap)
//   [self handleMediaEvent:event];
// }

- (BOOL) handleMediaEvent:(NSEvent*)event {
  // if it is not a media key event, then ignore
  if ([event type] == NSEventTypeSystemDefined && [event subtype] == 8) {
    int keyCode = (([event data1] & 0xFFFF0000) >> 16);
    int keyFlags = ([event data1] & 0x0000FFFF);
    int keyIsReleased = (((keyFlags & 0xFF00) >> 8)) == 0xB;
    if (keyIsReleased) {
      shortcut_handler_->MacMediaKeyPressed(keyCode);
      return YES;
    }
  }

  return NO;
}

- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication*) sender {
  Q_UNUSED(sender);
  return NSTerminateNow;
}

- (BOOL) userNotificationCenter: (id)center shouldPresentNotification: (id)notification {
  Q_UNUSED(center);
  Q_UNUSED(notification);
  // Always show notifications, even if Strawberry is in the foreground.
  return YES;
}

@end

@implementation MacApplication

- (id)init {
  if ((self = [super init])) {
    [self SetShortcutHandler:nil];
  }
  return self;
}

- (GlobalShortcutsBackendMacOS*)shortcut_handler {
  // should be the same as delegate_'s shortcut handler
  return shortcut_handler_;
}

- (void)SetShortcutHandler:(GlobalShortcutsBackendMacOS*)handler {
  shortcut_handler_ = handler;
  if (delegate_) [delegate_ setShortcutHandler:handler];
}

- (PlatformInterface*)application_handler {
  return application_handler_;
}

- (void)SetApplicationHandler:(PlatformInterface*)handler {

  delegate_ = [[AppDelegate alloc] initWithHandler:handler];
  // App-shortcut-handler set before delegate is set.
  // this makes sure the delegate's shortcut_handler is set
  [delegate_ setShortcutHandler:shortcut_handler_];
  [self setDelegate:delegate_];

  // FIXME
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  [[NSUserNotificationCenter defaultUserNotificationCenter]setDelegate:delegate_];
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

}

- (void)sendEvent:(NSEvent*)event {

  [delegate_ handleMediaEvent:event];
  [super sendEvent:event];

}

@end

namespace mac {

void MacMain() {

  ScopedNSAutoreleasePool pool;
  // Creates and sets the magic global variable so QApplication will find it.
  [MacApplication sharedApplication];

}

void SetShortcutHandler(GlobalShortcutsBackendMacOS *handler) {
  [NSApp SetShortcutHandler:handler];
}

void SetApplicationHandler(PlatformInterface *handler) {
  [NSApp SetApplicationHandler:handler];
}

static int MapFunctionKey(int keycode) {

  switch (keycode) {
    // Function keys
    case NSInsertFunctionKey: return Qt::Key_Insert;
    case NSDeleteFunctionKey: return Qt::Key_Delete;
    case NSPauseFunctionKey: return Qt::Key_Pause;
    case NSPrintFunctionKey: return Qt::Key_Print;
    case NSSysReqFunctionKey: return Qt::Key_SysReq;
    case NSHomeFunctionKey: return Qt::Key_Home;
    case NSEndFunctionKey: return Qt::Key_End;
    case NSLeftArrowFunctionKey: return Qt::Key_Left;
    case NSUpArrowFunctionKey: return Qt::Key_Up;
    case NSRightArrowFunctionKey: return Qt::Key_Right;
    case NSDownArrowFunctionKey: return Qt::Key_Down;
    case NSPageUpFunctionKey: return Qt::Key_PageUp;
    case NSPageDownFunctionKey: return Qt::Key_PageDown;
    case NSScrollLockFunctionKey: return Qt::Key_ScrollLock;
    case NSF1FunctionKey: return Qt::Key_F1;
    case NSF2FunctionKey: return Qt::Key_F2;
    case NSF3FunctionKey: return Qt::Key_F3;
    case NSF4FunctionKey: return Qt::Key_F4;
    case NSF5FunctionKey: return Qt::Key_F5;
    case NSF6FunctionKey: return Qt::Key_F6;
    case NSF7FunctionKey: return Qt::Key_F7;
    case NSF8FunctionKey: return Qt::Key_F8;
    case NSF9FunctionKey: return Qt::Key_F9;
    case NSF10FunctionKey: return Qt::Key_F10;
    case NSF11FunctionKey: return Qt::Key_F11;
    case NSF12FunctionKey: return Qt::Key_F12;
    case NSF13FunctionKey: return Qt::Key_F13;
    case NSF14FunctionKey: return Qt::Key_F14;
    case NSF15FunctionKey: return Qt::Key_F15;
    case NSF16FunctionKey: return Qt::Key_F16;
    case NSF17FunctionKey: return Qt::Key_F17;
    case NSF18FunctionKey: return Qt::Key_F18;
    case NSF19FunctionKey: return Qt::Key_F19;
    case NSF20FunctionKey: return Qt::Key_F20;
    case NSF21FunctionKey: return Qt::Key_F21;
    case NSF22FunctionKey: return Qt::Key_F22;
    case NSF23FunctionKey: return Qt::Key_F23;
    case NSF24FunctionKey: return Qt::Key_F24;
    case NSF25FunctionKey: return Qt::Key_F25;
    case NSF26FunctionKey: return Qt::Key_F26;
    case NSF27FunctionKey: return Qt::Key_F27;
    case NSF28FunctionKey: return Qt::Key_F28;
    case NSF29FunctionKey: return Qt::Key_F29;
    case NSF30FunctionKey: return Qt::Key_F30;
    case NSF31FunctionKey: return Qt::Key_F31;
    case NSF32FunctionKey: return Qt::Key_F32;
    case NSF33FunctionKey: return Qt::Key_F33;
    case NSF34FunctionKey: return Qt::Key_F34;
    case NSF35FunctionKey: return Qt::Key_F35;
    case NSMenuFunctionKey: return Qt::Key_Menu;
    case NSHelpFunctionKey: return Qt::Key_Help;
  }

  return 0;
}

QKeySequence KeySequenceFromNSEvent(NSEvent *event) {

  NSString *str = [event charactersIgnoringModifiers];
  NSString *upper = [str uppercaseString];
  const char *chars = [upper UTF8String];
  NSUInteger modifiers = [event modifierFlags];
  int key = 0;
  unsigned char c = chars[0];
  switch (c) {
    case 0x1b: key = Qt::Key_Escape; break;
    case 0x09: key = Qt::Key_Tab; break;
    case 0x0d: key = Qt::Key_Return; break;
    case 0x08: key = Qt::Key_Backspace; break;
    case 0x03: key = Qt::Key_Enter; break;
  }

  if (key == 0) {
    if (c >= 0x20 && c <= 0x7e) {  // ASCII from space to ~
      key = c;
    }
    else {
      key = MapFunctionKey([event keyCode]);
      if (key == 0) {
        return QKeySequence();
      }
    }
  }

  if (modifiers & NSEventModifierFlagShift) {
    key += Qt::SHIFT;
  }
  if (modifiers & NSEventModifierFlagControl) {
    key += Qt::META;
  }
  if (modifiers & NSEventModifierFlagOption) {
    key += Qt::ALT;
  }
  if (modifiers & NSEventModifierFlagCommand) {
    key += Qt::CTRL;
  }

  return QKeySequence(key);

}

void DumpDictionary(CFDictionaryRef dict) {

  NSDictionary *d = reinterpret_cast<NSDictionary*>(dict);
  NSLog(@"%@", d);

}

// NSWindowCollectionBehaviorFullScreenPrimary
static const NSUInteger kFullScreenPrimary = 1 << 7;

void EnableFullScreen(const QWidget &main_window) {

  NSView *view = reinterpret_cast<NSView*>(main_window.winId());
  NSWindow *window = [view window];
  [window setCollectionBehavior:kFullScreenPrimary];

}

}  // namespace mac
