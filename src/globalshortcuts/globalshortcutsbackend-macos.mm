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

 #include "config.h"

 #include <QtGlobal>

#include "globalshortcutsbackend-macos.h"

#include <AppKit/NSEvent.h>
#include <AppKit/NSWorkspace.h>
#include <Foundation/NSString.h>
#include <IOKit/hidsystem/ev_keymap.h>
#include <ApplicationServices/ApplicationServices.h>

#include <QAction>
#include <QList>
#include <QMessageBox>
#include <QPushButton>

#include "globalshortcutsmanager.h"
#include "core/logging.h"
#include "core/mac_startup.h"

#import "includes/SBSystemPreferences.h"

class GlobalShortcutsBackendMacOSPrivate {
 public:
  explicit GlobalShortcutsBackendMacOSPrivate(GlobalShortcutsBackendMacOS *backend)
      : global_monitor_(nil), local_monitor_(nil), backend_(backend) {}

  bool Register() {
    global_monitor_ = [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^(NSEvent *event) {
      HandleKeyEvent(event);
    } ];
    local_monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^(NSEvent *event) {
      // Filter event if we handle it as a global shortcut.
      return HandleKeyEvent(event) ? nil : event;
    } ];
    return true;
  }

  void Unregister() {
    [NSEvent removeMonitor:global_monitor_];
    [NSEvent removeMonitor:local_monitor_];
  }

 private:
  bool HandleKeyEvent(NSEvent *event) {
    QKeySequence sequence = mac::KeySequenceFromNSEvent(event);
    return backend_->KeyPressed(sequence);
  }

  id global_monitor_;
  id local_monitor_;
  GlobalShortcutsBackendMacOS *backend_;

  Q_DISABLE_COPY(GlobalShortcutsBackendMacOSPrivate)
};

GlobalShortcutsBackendMacOS::GlobalShortcutsBackendMacOS(GlobalShortcutsManager *manager, QObject *parent)
    : GlobalShortcutsBackend(manager, GlobalShortcutsBackend::Type::macOS, parent),
      p_(new GlobalShortcutsBackendMacOSPrivate(this)) {}

GlobalShortcutsBackendMacOS::~GlobalShortcutsBackendMacOS() {}

bool GlobalShortcutsBackendMacOS::DoRegister() {

  // Always enable media keys.
  mac::SetShortcutHandler(this);

  QList<GlobalShortcutsManager::Shortcut> shortcuts = manager_->shortcuts().values();
  for (const GlobalShortcutsManager::Shortcut &shortcut : shortcuts) {
    shortcuts_[shortcut.action->shortcut()] = shortcut.action;
  }

  return p_->Register();

}

void GlobalShortcutsBackendMacOS::DoUnregister() {

  p_->Unregister();
  shortcuts_.clear();

}

void GlobalShortcutsBackendMacOS::MacMediaKeyPressed(const int key) {

  switch (key) {
    case NX_KEYTYPE_PLAY:
      KeyPressed(Qt::Key_MediaPlay);
      break;
    case NX_KEYTYPE_FAST:
      KeyPressed(Qt::Key_MediaNext);
      break;
    case NX_KEYTYPE_REWIND:
      KeyPressed(Qt::Key_MediaPrevious);
      break;
  }

}

bool GlobalShortcutsBackendMacOS::KeyPressed(const QKeySequence &sequence) {

  if (sequence.isEmpty()) {
    return false;
  }
  QAction *action = shortcuts_[sequence];
  if (action) {
    action->trigger();
    return true;
  }
  return false;

}

bool GlobalShortcutsBackendMacOS::IsAccessibilityEnabled() {

  NSDictionary *options = @{reinterpret_cast<id>(kAXTrustedCheckOptionPrompt): @YES};
  return AXIsProcessTrustedWithOptions(reinterpret_cast<CFDictionaryRef>(options));

}

void GlobalShortcutsBackendMacOS::ShowAccessibilityDialog() {

  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSPreferencePanesDirectory, NSSystemDomainMask, YES);
  if ([paths count] == 1) {
    SBSystemPreferencesApplication *system_prefs = [SBApplication applicationWithBundleIdentifier:@"com.apple.systempreferences"];
    [system_prefs activate];

    SBElementArray *panes = [system_prefs panes];
    SBSystemPreferencesPane *security_pane = nil;
    for (SBSystemPreferencesPane *pane : panes) {
      if ([[pane id] isEqualToString:@"com.apple.preference.security"]) {
        security_pane = pane;
        break;
      }
    }
    [system_prefs setCurrentPane:security_pane];

    SBElementArray *anchors = [security_pane anchors];
    for (SBSystemPreferencesAnchor *anchor : anchors) {
      if ([[anchor name] isEqualToString:@"Privacy_Accessibility"]) {
        [anchor reveal];
      }
    }
  }

}
