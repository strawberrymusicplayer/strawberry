/*
 * Strawberry Music Player
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

#ifndef STREAMPLAYLISTITEM_H
#define STREAMPLAYLISTITEM_H

#include <QVariant>
#include <QUrl>

#include "core/song.h"
#include "core/sqlrow.h"
#include "playlist/playlistitem.h"

class StreamPlaylistItem : public PlaylistItem {

 public:
  explicit StreamPlaylistItem(const Song::Source source);
  explicit StreamPlaylistItem(const Song &song);

  Song OriginalMetadata() const override { return song_; }
  QUrl OriginalUrl() const override { return song_.url(); }
  void SetOriginalMetadata(const Song &song) override { song_ = song; }
  bool InitFromQuery(const SqlRow &query) override;
  void SetArtManual(const QUrl &cover_url) override;

 protected:
  QVariant DatabaseValue(const DatabaseColumn column) const override;
  Song DatabaseSongMetadata() const override { return song_; }

 private:
  void InitMetadata();

 private:
  Song::Source source_;
  Song song_;

  Q_DISABLE_COPY(StreamPlaylistItem)
};

#endif  // STREAMPLAYLISTITEM_H
