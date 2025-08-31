/*
  *Strawberry Music Player
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

#ifndef MACSYSTEMTRAYICON_H
#define MACSYSTEMTRAYICON_H

#include "config.h"

#include <QObject>
#include <QUrl>
#include <QPixmap>
#include <QAction>

#include "includes/scoped_ptr.h"
#include "core/song.h"

class MacSystemTrayIconPrivate;

class SystemTrayIcon : public QObject {
  Q_OBJECT

 public:
  explicit SystemTrayIcon(QObject *parent = nullptr);
  ~SystemTrayIcon();

  bool isSystemTrayAvailable() const { return true; }
  bool IsSystemTrayAvailable() const { return true; }

  bool isVisible() const { return true; }
  void setVisible(const bool) {}

  void SetDevicePixelRatioF(const qreal device_pixel_ratio);
  void SetTrayiconProgress(const bool enabled);

  void SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *love, QAction *quit);
  void ShowPopup(const QString&, const QString&, const int) {}

  bool MuteEnabled() const { return false; }
  void SetMuteEnabled(const bool) {}
  void MuteButtonStateChanged(const bool) {}

  void SetPlaying(bool enable_play_pause = false);
  void SetPaused();
  void SetStopped();

  void SetNowPlaying(const Song &song, const QUrl&);
  void ClearNowPlaying();

  void SetProgress(const int percentage);

  void LoveVisibilityChanged(const bool) {}
  void LoveStateChanged(const bool) {}

 private:
  void SetupMenuItem(QAction *action);
  QPixmap CreateIcon(const QPixmap &icon, const QPixmap &grey_icon);
  void UpdateIcon();

 private Q_SLOTS:
  void ActionChanged();

 Q_SIGNALS:
  void ChangeVolume(const int delta);
  void SeekForward();
  void SeekBackward();
  void NextTrack();
  void PreviousTrack();
  void ShowHide();
  void PlayPause();

 private:
  ScopedPtr<MacSystemTrayIconPrivate> p_;

  QPixmap normal_icon_;
  QPixmap grey_icon_;
  QPixmap playing_icon_;
  QPixmap paused_icon_;
  QPixmap current_state_icon_;
  qreal device_pixel_ratio_;
  bool trayicon_progress_;
  int song_progress_;
  Q_DISABLE_COPY(SystemTrayIcon);
};

#endif  // MACSYSTEMTRAYICON_H
