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

#ifndef SONGPLAYLISTITEM_H
#define SONGPLAYLISTITEM_H

#include "config.h"

#include <QUrl>

#include "core/song.h"
#include "core/sqlrow.h"
#include "playlistitem.h"

class SongPlaylistItem : public PlaylistItem {
 public:
  explicit SongPlaylistItem(const Song::Source source);
  explicit SongPlaylistItem(const Song &song);

  // Restores a stream- or file-related playlist item using query row.
  // If it's a file related playlist item, this will restore its CUE attributes (if any) but won't parse the CUE!
  bool InitFromQuery(const SqlRow &query) override;
  void Reload() override;

  Song OriginalMetadata() const override { return song_; }
  QUrl OriginalUrl() const override { return song_.url(); }

  void SetArtManual(const QUrl &cover_url) override;

 protected:
  Song DatabaseSongMetadata() const override { return song_; }

 private:
  Song song_;

  Q_DISABLE_COPY(SongPlaylistItem)
};

#endif  // SONGPLAYLISTITEM_H
