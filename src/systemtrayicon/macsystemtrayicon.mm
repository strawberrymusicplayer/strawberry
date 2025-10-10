/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QApplication>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QIcon>
#include <QAction>

#include <AppKit/NSMenu.h>
#include <AppKit/NSMenuItem.h>

#include "macsystemtrayicon.h"

#include "includes/mac_delegate.h"

#include "core/song.h"
#include "core/iconloader.h"

using namespace Qt::Literals::StringLiterals;

@interface Target :NSObject {
  QAction *action_;
}
- (id) initWithQAction: (QAction*)action;
- (void) clicked;
@end

@implementation Target  // <NSMenuValidation>
- (id) init {
  return [super init];
}

- (id) initWithQAction: (QAction*)action {
  action_ = action;
  return self;
}

- (BOOL) validateMenuItem: (NSMenuItem*)menuItem {
  Q_UNUSED(menuItem);
  // This is called when the menu is shown.
  return action_->isEnabled();
}

- (void) clicked {
  action_->trigger();
}
@end

class MacSystemTrayIconPrivate {
 public:
  MacSystemTrayIconPrivate() {
    dock_menu_ = [[NSMenu alloc] initWithTitle:@"DockMenu"];

    QString title = QT_TR_NOOP(u"Now Playing"_s);
    NSString *t = [[NSString alloc] initWithUTF8String:title.toUtf8().constData()];
    now_playing_ = [[NSMenuItem alloc] initWithTitle:t action:nullptr keyEquivalent:@""];
    now_playing_artist_ = [[NSMenuItem alloc] initWithTitle:@"Nothing to see here" action:nullptr keyEquivalent:@""];
    now_playing_title_ = [[NSMenuItem alloc] initWithTitle:@"Nothing to see here" action:nullptr keyEquivalent:@""];

    [dock_menu_ insertItem:now_playing_title_ atIndex:0];
    [dock_menu_ insertItem:now_playing_artist_ atIndex:0];
    [dock_menu_ insertItem:now_playing_ atIndex:0];

    // Don't look now.
    // This must be called after our custom NSApplicationDelegate has been set.
    [reinterpret_cast<AppDelegate*>([NSApp delegate]) setDockMenu:dock_menu_];

    ClearNowPlaying();
  }

  void AddMenuItem(QAction *action) {
    // Strip accelarators from name.
    QString text = action->text().remove(u'&');
    NSString *title = [[NSString alloc] initWithUTF8String: text.toUtf8().constData()];
    // Create an object that can receive user clicks and pass them on to the QAction.
    Target *target = [[Target alloc] initWithQAction:action];
    NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:title action:@selector(clicked) keyEquivalent:@""] autorelease];
    [item setEnabled:action->isEnabled()];
    [item setTarget:target];
    [dock_menu_ addItem:item];
    actions_[action] = item;
  }

  void ActionChanged(QAction *action) {
    NSMenuItem *item = actions_[action];
    NSString *title = [[NSString alloc] initWithUTF8String: action->text().toUtf8().constData()];
    [item setTitle:title];
  }

  void AddSeparator() {
    NSMenuItem *separator = [NSMenuItem separatorItem];
    [dock_menu_ addItem:separator];
  }

  void ShowNowPlaying(const QString &artist, const QString &title) {
    ClearNowPlaying();  // Makes sure the order is consistent.
    [now_playing_artist_ setTitle: [[NSString alloc] initWithUTF8String: artist.toUtf8().constData()]];
    [now_playing_title_ setTitle: [[NSString alloc] initWithUTF8String: title.toUtf8().constData()]];
    title.isEmpty() ? HideItem(now_playing_title_) : ShowItem(now_playing_title_);
    artist.isEmpty() ? HideItem(now_playing_artist_) : ShowItem(now_playing_artist_);
    artist.isEmpty() && title.isEmpty() ? HideItem(now_playing_) : ShowItem(now_playing_);
  }

  void ClearNowPlaying() {
    // Hiding doesn't seem to work in the dock menu.
    HideItem(now_playing_);
    HideItem(now_playing_artist_);
    HideItem(now_playing_title_);
  }

 private:
  void HideItem(NSMenuItem *item) {
    if ([dock_menu_ indexOfItem:item] != -1) {
      [dock_menu_ removeItem:item];
    }
  }

  void ShowItem(NSMenuItem *item, int index = 0) {
    if ([dock_menu_ indexOfItem:item] == -1) {
      [dock_menu_ insertItem:item atIndex:index];
    }
  }

  QHash<QAction*, NSMenuItem*> actions_;

  NSMenu *dock_menu_;
  NSMenuItem *now_playing_;
  NSMenuItem *now_playing_artist_;
  NSMenuItem *now_playing_title_;

  Q_DISABLE_COPY(MacSystemTrayIconPrivate);
};

SystemTrayIcon::SystemTrayIcon(QObject *parent)
    : QObject(parent),
      normal_icon_(QPixmap(u":/pictures/strawberry.png"_s).scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)),
      grey_icon_(QPixmap(u":/pictures/strawberry-grey.png"_s).scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)),
      playing_icon_(u":/pictures/tiny-play.png"_s),
      paused_icon_(u":/pictures/tiny-pause.png"_s),
      device_pixel_ratio_(1.0),
      trayicon_progress_(false),
      song_progress_(0) {

  QApplication::setWindowIcon(normal_icon_);

}

SystemTrayIcon::~SystemTrayIcon() {}

void SystemTrayIcon::SetDevicePixelRatioF(const qreal device_pixel_ratio) {

  device_pixel_ratio_ = device_pixel_ratio;

}

void SystemTrayIcon::SetTrayiconProgress(const bool enabled) {

  trayicon_progress_ = enabled;
  UpdateIcon();

}

void SystemTrayIcon::SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *love, QAction *quit) {

  p_ = std::make_unique<MacSystemTrayIconPrivate>();

  SetupMenuItem(previous);
  SetupMenuItem(play);
  SetupMenuItem(stop);
  SetupMenuItem(stop_after);
  SetupMenuItem(next);
  p_->AddSeparator();
  SetupMenuItem(mute);
  p_->AddSeparator();
  SetupMenuItem(love);
  Q_UNUSED(quit);  // Mac already has a Quit item.

}

void SystemTrayIcon::SetupMenuItem(QAction *action) {
  p_->AddMenuItem(action);
  QObject::connect(action, &QAction::changed, this, &SystemTrayIcon::ActionChanged);
}

void SystemTrayIcon::UpdateIcon() {

  QApplication::setWindowIcon(CreateIcon(normal_icon_, grey_icon_));

}

void SystemTrayIcon::ActionChanged() {

  QAction *action = qobject_cast<QAction*>(sender());
  p_->ActionChanged(action);

}

void SystemTrayIcon::SetPlaying(const bool enable_play_pause) {

  Q_UNUSED(enable_play_pause);

  current_state_icon_ = playing_icon_;
  UpdateIcon();

}

void SystemTrayIcon::SetPaused() {

  current_state_icon_ = paused_icon_;
  UpdateIcon();

}

void SystemTrayIcon::SetStopped() {

  current_state_icon_ = QPixmap();
  UpdateIcon();

}

void SystemTrayIcon::SetProgress(const int percentage) {

  song_progress_ = percentage;
  if (trayicon_progress_) UpdateIcon();

}

void SystemTrayIcon::ClearNowPlaying() {
  p_->ClearNowPlaying();
}

void SystemTrayIcon::SetNowPlaying(const Song &song, const QUrl&) {
  p_->ShowNowPlaying(song.artist(), song.PrettyTitle());
}
