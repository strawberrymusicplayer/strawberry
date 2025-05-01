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

#ifndef COLLECTIONPLAYLISTITEM_H
#define COLLECTIONPLAYLISTITEM_H

#include "config.h"

#include <QVariant>
#include <QUrl>

#include "core/song.h"
#include "playlist/playlistitem.h"

class SqlRow;

class CollectionPlaylistItem : public PlaylistItem {
 public:
  explicit CollectionPlaylistItem(const Song::Source source);
  explicit CollectionPlaylistItem(const Song &song);

  Song OriginalMetadata() const override { return song_; }
  void SetOriginalMetadata(const Song &song) override { song_ = song; }

  QUrl OriginalUrl() const override { return song_.url(); }
  bool IsLocalCollectionItem() const override { return song_.source() == Song::Source::Collection; }

  bool InitFromQuery(const SqlRow &query) override;
  void Reload() override;

  void SetArtManual(const QUrl &cover_url) override;

 protected:
  QVariant DatabaseValue(const DatabaseColumn database_column) const override;
  Song DatabaseSongMetadata() const override { return Song(source_); }

 protected:
  Song song_;

 private:
  Q_DISABLE_COPY(CollectionPlaylistItem)
};

#endif  // COLLECTIONPLAYLISTITEM_H

