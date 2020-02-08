/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef SYSTEMTRAYICON_H
#define SYSTEMTRAYICON_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QPixmap>

class QAction;

class Song;

class SystemTrayIcon : public QObject {
  Q_OBJECT

 public:
  SystemTrayIcon(QObject *parent = nullptr);

  // Called once to create the icon's context menu
  virtual void SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *love, QAction *quit) = 0;

  virtual bool IsVisible() const { return true; }
  virtual void SetVisible(bool visible) { Q_UNUSED(visible); }

  // Called by the OSD
  virtual void ShowPopup(const QString &summary, const QString &message, int timeout) { Q_UNUSED(summary); Q_UNUSED(message); Q_UNUSED(timeout); }
  // If this gets invoked with image_path equal to nullptr, the tooltip should still be shown - just without the cover art.
  virtual void SetNowPlaying(const Song &song, const QUrl &cover_url) { Q_UNUSED(song); Q_UNUSED(cover_url); }
  virtual void ClearNowPlaying() {}

  virtual bool MuteEnabled() { return false; }
  virtual void SetMuteEnabled(bool enabled) { Q_UNUSED(enabled); }

  static SystemTrayIcon *CreateSystemTrayIcon(QObject *parent = nullptr);

 public slots:
  void SetProgress(int percentage);
  virtual void SetPaused();
  virtual void SetPlaying(bool enable_play_pause = false);
  virtual void SetStopped();
  virtual void LoveVisibilityChanged(bool value) { Q_UNUSED(value); }
  virtual void LoveStateChanged(bool value) { Q_UNUSED(value); }
  virtual void MuteButtonStateChanged(bool value) { Q_UNUSED(value); }

 signals:
  void ChangeVolume(int delta);
  void SeekForward();
  void SeekBackward();
  void NextTrack();
  void PreviousTrack();
  void ShowHide();
  void PlayPause();

 protected:
  virtual void UpdateIcon() = 0;
  QPixmap CreateIcon(const QPixmap &icon, const QPixmap &grey_icon);

  int song_progress() const { return percentage_; }
  QPixmap current_state_icon() const { return current_state_icon_; }

 private:
  int percentage_;
  QPixmap playing_icon_;
  QPixmap paused_icon_;
  QPixmap current_state_icon_;
};

#endif  // SYSTEMTRAYICON_H
