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

#include "config.h"

#include <QUrl>

#include "core/logging.h"
#include "core/song.h"
#include "core/sqlrow.h"
#include "tagreader/tagreaderclient.h"
#include "playlistitem.h"
#include "songplaylistitem.h"

SongPlaylistItem::SongPlaylistItem(const Song::Source source) : PlaylistItem(source) {}
SongPlaylistItem::SongPlaylistItem(const Song &song) : PlaylistItem(song.source()), song_(song) {}

bool SongPlaylistItem::InitFromQuery(const SqlRow &query) {
  song_.InitFromQuery(query, false, static_cast<int>(Song::kRowIdColumns.count()));
  return true;
}

void SongPlaylistItem::Reload() {

  if (!song_.url().isLocalFile()) return;

  const TagReaderResult result = TagReaderClient::Instance()->ReadFileBlocking(song_.url().toLocalFile(), &song_);
  if (!result.success()) {
    qLog(Error) << "Could not reload file" << song_.url() << result.error_string();
  }

  UpdateStreamMetadata(song_);

}

void SongPlaylistItem::SetArtManual(const QUrl &cover_url) {

  song_.set_art_manual(cover_url);
  if (HasStreamMetadata()) stream_song_.set_art_manual(cover_url);

}
