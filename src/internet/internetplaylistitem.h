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

#ifndef INTERNETPLAYLISTITEM_H
#define INTERNETPLAYLISTITEM_H

#include "config.h"

#include <QVariant>
#include <QUrl>

#include "core/song.h"
#include "collection/sqlrow.h"
#include "playlist/playlistitem.h"

class InternetService;

class InternetPlaylistItem : public PlaylistItem {

 public:
  explicit InternetPlaylistItem(const Song::Source source);
  explicit InternetPlaylistItem(const Song &metadata);
  explicit InternetPlaylistItem(InternetService *service, const Song &metadata);

  bool InitFromQuery(const SqlRow &query) override;
  Song Metadata() const override;
  Song OriginalMetadata() const override { return metadata_; }
  QUrl Url() const override;
  void SetArtManual(const QUrl &cover_url) override;

 protected:
  QVariant DatabaseValue(DatabaseColumn) const override;
  Song DatabaseSongMetadata() const override { return metadata_; }

 private:
  void InitMetadata();

 private:
  Song::Source source_;
  Song metadata_;
};

#endif
