/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTROWSMIMEDATA_H
#define PLAYLISTROWSMIMEDATA_H

#include "config.h"

#include <QObject>
#include <QMimeData>
#include <QList>
#include <QModelIndexList>
#include <QPersistentModelIndex>
#include <QPointer>

class Playlist;

// The rows dragged from a playlist.
// The rows can change during the drag, like when a dynamic playlist removes the history when the track changes, so the dragged rows are followed.
class PlaylistRowsMimeData : public QMimeData {
  Q_OBJECT

 public:
  explicit PlaylistRowsMimeData(Playlist *playlist, const QModelIndexList &indexes, QObject *parent = nullptr);

  // The playlist the rows were dragged from, null if it was closed during the drag.
  Playlist *playlist() const;

  // The rows of the dragged items now, rows which were removed during the drag are left out.
  QList<int> rows() const;

 private:
  QPointer<Playlist> playlist_;
  QList<QPersistentModelIndex> indexes_;
};

#endif  // PLAYLISTROWSMIMEDATA_H
