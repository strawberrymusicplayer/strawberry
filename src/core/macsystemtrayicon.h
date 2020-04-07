/*
  *Strawberry Music Player
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

#ifndef MACSYSTEMTRAYICON_H
#define MACSYSTEMTRAYICON_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QUrl>
#include <QPixmap>
#include <QAction>

#include "systemtrayicon.h"

class MacSystemTrayIconPrivate;

class MacSystemTrayIcon : public SystemTrayIcon {
  Q_OBJECT

 public:
  explicit MacSystemTrayIcon(QObject *parent = nullptr);
  ~MacSystemTrayIcon();

  void SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *love, QAction *quit);

  void SetNowPlaying(const Song& song, const QUrl &cover_url);
  void ClearNowPlaying();

private:
  void SetupMenuItem(QAction *action);

private slots:
  void ActionChanged();

protected:
  // SystemTrayIcon
  void UpdateIcon();

private:
  QPixmap normal_icon_;
  std::unique_ptr<MacSystemTrayIconPrivate> p_;
  Q_DISABLE_COPY(MacSystemTrayIcon);
};

#endif  // MACSYSTEMTRAYICON_H
