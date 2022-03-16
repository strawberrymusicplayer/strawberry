/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef RADIOPLAYLISTITEM_H
#define RADIOPLAYLISTITEM_H

#include "config.h"

#include <QVariant>
#include <QUrl>

#include "core/song.h"
#include "collection/sqlrow.h"
#include "playlist/playlistitem.h"

class RadioService;

class RadioPlaylistItem : public PlaylistItem {

 public:
  explicit RadioPlaylistItem(const Song::Source source);
  explicit RadioPlaylistItem(const Song &metadata);

  bool InitFromQuery(const SqlRow &query) override;
  Song Metadata() const override;
  Song OriginalMetadata() const override { return metadata_; }
  QUrl Url() const override;

  void SetMetadata(const Song &metadata) override { metadata_ = metadata; }
  void SetArtManual(const QUrl &cover_url) override;

 protected:
  QVariant DatabaseValue(DatabaseColumn) const override;
  Song DatabaseSongMetadata() const override { return metadata_; }

 private:
  void InitMetadata();

 private:
  Song::Source source_;
  Song metadata_;

  Q_DISABLE_COPY(RadioPlaylistItem)

};

#endif  // INTERNETPLAYLISTITEM_H
