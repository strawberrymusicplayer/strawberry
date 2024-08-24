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

#include <QObject>
#include <QCoreApplication>
#include <QSystemTrayIcon>
#include <QAction>
#include <QMenu>
#include <QIcon>
#include <QString>
#include <QUrl>

#include "song.h"
#include "iconloader.h"
#include "qtsystemtrayicon.h"

SystemTrayIcon::SystemTrayIcon(QObject *parent)
    : QSystemTrayIcon(parent),
      menu_(new QMenu),
      app_name_(QCoreApplication::applicationName()),
      pixmap_playing_(QStringLiteral(":/pictures/tiny-play.png")),
      pixmap_paused_(QStringLiteral(":/pictures/tiny-pause.png")),
      action_play_pause_(nullptr),
      action_stop_(nullptr),
      action_stop_after_this_track_(nullptr),
      action_mute_(nullptr),
      action_love_(nullptr),
      available_(false),
      trayicon_progress_(false),
      song_progress_(0) {

  app_name_[0] = app_name_[0].toUpper();

  const QIcon icon = IconLoader::Load(QStringLiteral("strawberry"));
  const QIcon icon_grey = IconLoader::Load(QStringLiteral("strawberry-grey"));
  pixmap_normal_ = icon.pixmap(48, QIcon::Normal);
  if (icon_grey.isNull()) {
    pixmap_grey_ = icon.pixmap(48, QIcon::Disabled);
  }
  else {
    pixmap_grey_ = icon_grey.pixmap(48, QIcon::Disabled);
  }

  if (isSystemTrayAvailable()) {
    available_ = true;
    setIcon(icon);
    setToolTip(app_name_);
  }

  QObject::connect(this, &QSystemTrayIcon::activated, this, &SystemTrayIcon::Clicked);

}

SystemTrayIcon::~SystemTrayIcon() {
  delete menu_;
}

void SystemTrayIcon::SetTrayiconProgress(const bool enabled) {

  trayicon_progress_ = enabled;
  UpdateIcon();

}

void SystemTrayIcon::SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *love, QAction *quit) {

  // Creating new actions and connecting them to old ones.
  // This allows us to use old actions without displaying shortcuts that can not be used when Strawberry's window is hidden
  menu_->addAction(previous->icon(), previous->text(), previous, &QAction::trigger);
  action_play_pause_ = menu_->addAction(play->icon(), play->text(), play, &QAction::trigger);
  action_stop_ = menu_->addAction(stop->icon(), stop->text(), stop, &QAction::trigger);
  action_stop_after_this_track_ = menu_->addAction(stop_after->icon(), stop_after->text(), stop_after, &QAction::trigger);
  menu_->addAction(next->icon(), next->text(), next, &QAction::trigger);

  menu_->addSeparator();
  action_mute_ = menu_->addAction(mute->icon(), mute->text(), mute, &QAction::trigger);
  action_mute_->setCheckable(true);
  action_mute_->setChecked(mute->isChecked());

  menu_->addSeparator();
  action_love_ = menu_->addAction(love->icon(), love->text(), love, &QAction::trigger);
  action_love_->setVisible(love->isVisible());
  action_love_->setEnabled(love->isEnabled());
  menu_->addSeparator();
  menu_->addAction(quit->icon(), quit->text(), quit, &QAction::trigger);

  if (available_) setContextMenu(menu_);

}

void SystemTrayIcon::Clicked(const QSystemTrayIcon::ActivationReason reason) {

  switch (reason) {
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::Trigger:
      Q_EMIT ShowHide();
      break;

    case QSystemTrayIcon::MiddleClick:
      Q_EMIT PlayPause();
      break;

    default:
      break;
  }

}

void SystemTrayIcon::ShowPopup(const QString &summary, const QString &message, const int timeout) {
  if (available_) showMessage(summary, message, QSystemTrayIcon::NoIcon, timeout);
}

void SystemTrayIcon::UpdateIcon() {

  if (available_) setIcon(CreateIcon(pixmap_normal_, pixmap_grey_));

}

void SystemTrayIcon::SetPlaying(bool enable_play_pause) {

  current_state_icon_ = pixmap_playing_;
  UpdateIcon();

  action_stop_->setEnabled(true);
  action_stop_after_this_track_->setEnabled(true);
  action_play_pause_->setIcon(IconLoader::Load(QStringLiteral("media-playback-pause")));
  action_play_pause_->setText(tr("Pause"));
  action_play_pause_->setEnabled(enable_play_pause);

}

void SystemTrayIcon::SetPaused() {

  current_state_icon_ = pixmap_paused_;
  UpdateIcon();

  action_stop_->setEnabled(true);
  action_stop_after_this_track_->setEnabled(true);
  action_play_pause_->setIcon(IconLoader::Load(QStringLiteral("media-playback-start")));
  action_play_pause_->setText(tr("Play"));

  action_play_pause_->setEnabled(true);

}

void SystemTrayIcon::SetStopped() {

  current_state_icon_ = QPixmap();
  UpdateIcon();

  action_stop_->setEnabled(false);
  action_stop_after_this_track_->setEnabled(false);
  action_play_pause_->setIcon(IconLoader::Load(QStringLiteral("media-playback-start")));
  action_play_pause_->setText(tr("Play"));

  action_play_pause_->setEnabled(true);

  action_love_->setEnabled(false);

}

void SystemTrayIcon::SetProgress(const int percentage) {

  song_progress_ = percentage;
  UpdateIcon();

}

void SystemTrayIcon::MuteButtonStateChanged(const bool value) {
  if (action_mute_) action_mute_->setChecked(value);
}

void SystemTrayIcon::SetNowPlaying(const Song &song, const QUrl&) {
  if (available_) setToolTip(song.PrettyTitleWithArtist());
}

void SystemTrayIcon::ClearNowPlaying() {
  if (available_) setToolTip(app_name_);
}

void SystemTrayIcon::LoveVisibilityChanged(const bool value) {
  if (action_love_) action_love_->setVisible(value);
}

void SystemTrayIcon::LoveStateChanged(const bool value) {
  if (action_love_) action_love_->setEnabled(value);
}
