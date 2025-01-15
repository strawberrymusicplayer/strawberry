/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#import <Sparkle/Sparkle.h>

#include <QObject>
#include <QAction>

#include "sparkleupdater.h"

@interface AppUpdaterDelegate : NSObject <SPUUpdaterDelegate>

@property(nonatomic, assign) SPUStandardUpdaterController *updater_controller;

@end

@implementation AppUpdaterDelegate

- (void)observeCanCheckForUpdatesWithAction:(QAction*)action_check_updates {
  [_updater_controller.updater addObserver:self forKeyPath:NSStringFromSelector(@selector(canCheckForUpdates)) options:(NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew) context:(void*)action_check_updates];
}

- (void)observeValueForKeyPath:(NSString*)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id>*)change context:(void*)context {

  if ([keyPath isEqualToString:NSStringFromSelector(@selector(canCheckForUpdates))]) {
    QAction *action = reinterpret_cast<QAction*>(context);
    action->setEnabled(_updater_controller.updater.canCheckForUpdates);
  }
  else {
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
  }

}

- (void)dealloc {

  @autoreleasepool {
    [_updater_controller.updater removeObserver:self forKeyPath:NSStringFromSelector(@selector(canCheckForUpdates))];
  }

  [super dealloc];

}

@end

SparkleUpdater::SparkleUpdater(QAction *action_check_updates, QObject *parent) : QObject(parent) {

  @autoreleasepool {
    updater_delegate_ = [[AppUpdaterDelegate alloc] init];
    updater_delegate_.updater_controller = [[SPUStandardUpdaterController alloc] initWithStartingUpdater:YES updaterDelegate:updater_delegate_ userDriverDelegate:nil];
    [updater_delegate_ observeCanCheckForUpdatesWithAction:action_check_updates];
  }

}

void SparkleUpdater::CheckForUpdates() {

  @autoreleasepool {
    [updater_delegate_.updater_controller checkForUpdates:nil];
  }

}
