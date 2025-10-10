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

#ifndef QTSYSTEMTRAYICON_H
#define QTSYSTEMTRAYICON_H

#include "config.h"

#include <QObject>
#include <QSystemTrayIcon>
#include <QString>
#include <QUrl>
#include <QIcon>
#include <QPixmap>
#include <QAction>
#include <QtEvents>

#include "core/song.h"

class QMenu;

class SystemTrayIcon : public QSystemTrayIcon {
  Q_OBJECT

 public:
  explicit SystemTrayIcon(QObject *parent = nullptr);
  ~SystemTrayIcon() override;

  bool IsSystemTrayAvailable() const { return available_; }

  void InitPixmaps();

  void SetDevicePixelRatioF(const qreal device_pixel_ratio);
  void SetTrayiconProgress(const bool enabled);

  void SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *love, QAction *quit);
  void ShowPopup(const QString &summary, const QString &message, const int timeout);

  void SetPlaying(const bool enable_play_pause = false);
  void SetPaused();
  void SetStopped();
  void SetProgress(const int percentage);
  void MuteButtonStateChanged(const bool value);
  void SetNowPlaying(const Song &song, const QUrl &url);
  void ClearNowPlaying();
  void LoveVisibilityChanged(const bool value);
  void LoveStateChanged(const bool value);

  bool MuteEnabled() const { return action_mute_->isVisible(); }
  void SetMuteEnabled(const bool enabled) { action_mute_->setVisible(enabled); }

 private:
  QPixmap CreateIcon(const QPixmap &icon, const QPixmap &grey_icon);
  void UpdateIcon();

 Q_SIGNALS:
  void ChangeVolume(const int delta);
  void SeekForward();
  void SeekBackward();
  void NextTrack();
  void PreviousTrack();
  void ShowHide();
  void PlayPause();

 private Q_SLOTS:
  void Clicked(const QSystemTrayIcon::ActivationReason);

 private:
  QMenu *menu_;

  QIcon icon_normal_;
  QIcon icon_grey_;
  QIcon icon_playing_;
  QIcon icon_paused_;

  QPixmap pixmap_normal_;
  QPixmap pixmap_grey_;
  QPixmap pixmap_playing_;
  QPixmap pixmap_paused_;

  QAction *action_play_pause_;
  QAction *action_stop_;
  QAction *action_stop_after_this_track_;
  QAction *action_mute_;
  QAction *action_love_;

  bool available_;
  qreal device_pixel_ratio_;
  bool trayicon_progress_;
  int song_progress_;

  QPixmap current_state_icon_;

};

#endif  // QTSYSTEMTRAYICON_H
