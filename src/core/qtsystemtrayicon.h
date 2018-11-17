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

#ifndef QTSYSTEMTRAYICON_H
#define QTSYSTEMTRAYICON_H

#include "config.h"

#include <stdbool.h>

#include <QObject>
#include <QSystemTrayIcon>
#include <QString>
#include <QPixmap>
#include <QAction>
#include <QMenu>
#include <QtEvents>

#include "systemtrayicon.h"

class QEvent;

class Song;

class QtSystemTrayIcon : public SystemTrayIcon {
  Q_OBJECT

 public:
  QtSystemTrayIcon(QObject *parent = nullptr);
  ~QtSystemTrayIcon();

  void SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *quit);
  bool IsVisible() const;
  void SetVisible(bool visible);

  void ShowPopup(const QString &summary, const QString &message, int timeout);

  void SetNowPlaying(const Song &song, const QString &image_path);
  void ClearNowPlaying();

protected:
  // SystemTrayIcon
  void UpdateIcon();
  void SetPaused();
  void SetPlaying(bool enable_play_pause = false);
  void SetStopped();
  void MuteButtonStateChanged(bool value);

  // QObject
  bool eventFilter(QObject *, QEvent *);

private slots:
  void Clicked(QSystemTrayIcon::ActivationReason);

private:
  QSystemTrayIcon *tray_;
  QMenu *menu_;
  QAction *action_play_pause_;
  QAction *action_stop_;
  QAction *action_stop_after_this_track_;
  QAction *action_mute_;

  QString pattern_;

  QPixmap normal_icon_;
  QPixmap grey_icon_;
};

#endif  // QTSYSTEMTRAYICON_H
