/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MIMEDATA_H
#define MIMEDATA_H

#include <QMimeData>
#include <QString>

class MimeData : public QMimeData {
  Q_OBJECT

 public:
  explicit MimeData(const bool clear = false, const bool play_now = false, const bool enqueue = false, const bool enqueue_next_now = false, const bool open_in_new_playlist = false, QObject *parent = nullptr);

  // If this is set then MainWindow will not touch any of the other flags.
  bool override_user_settings_;

  // If this is set then the playlist will be cleared before these songs are inserted.
  bool clear_first_;

  // If this is set then the first item that is inserted will start playing immediately.
  // Note: this is always overridden with the user's preference if the MimeData goes via MainWindow, unless you set override_user_settings_.
  bool play_now_;

  // If this is set then the items are added to the queue after being inserted.
  bool enqueue_now_;

  // If this is set then the items are added to the beginning of the queue after being inserted.
  bool enqueue_next_now_;

  // If this is set then the items are inserted into a newly created playlist.
  bool open_in_new_playlist_;

  // This serves as a name for the new playlist in 'open_in_new_playlist_' mode.
  QString name_for_new_playlist_;

  // This can be set if this MimeData goes via MainWindow (ie. it is created manually in a double-click).
  // The MainWindow will set the above flags to the defaults set by the user.
  bool from_doubleclick_;

  // Returns a pretty name for a playlist containing songs described by this MimeData object.
  // By pretty name we mean the value of 'name_for_new_playlist_' or generic "Playlist" string if the 'name_for_new_playlist_' attribute is empty.
  QString get_name_for_new_playlist() const;
};

#endif  // MIMEDATA_H
