/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QUrl>

#include "core/tagreaderclient.h"
#include "core/song.h"
#include "collection/sqlrow.h"
#include "playlistitem.h"
#include "songplaylistitem.h"

SongPlaylistItem::SongPlaylistItem(const Song::Source &source) : PlaylistItem(source) {}
SongPlaylistItem::SongPlaylistItem(const Song &song) : PlaylistItem(song.source()), song_(song) {}

bool SongPlaylistItem::InitFromQuery(const SqlRow &query) {
  song_.InitFromQuery(query, false, (Song::kColumns.count()+1));
  return true;
}

QUrl SongPlaylistItem::Url() const { return song_.url(); }

void SongPlaylistItem::Reload() {
  if (!song_.url().isLocalFile()) return;
  TagReaderClient::Instance()->ReadFileBlocking(song_.url().toLocalFile(), &song_);
}

Song SongPlaylistItem::Metadata() const {
  if (HasTemporaryMetadata()) return temp_metadata_;
  return song_;
}
