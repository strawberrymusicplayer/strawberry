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

#include "config.h"

#include <QApplication>
#include <QVariant>

#include "streamplaylistitem.h"
#include "core/sqlrow.h"

StreamPlaylistItem::StreamPlaylistItem(const Song::Source source)
    : PlaylistItem(source),
      source_(source) {}

StreamPlaylistItem::StreamPlaylistItem(const Song &song)
    : PlaylistItem(song.source()),
      source_(song.source()),
      song_(song) {
  InitMetadata();
}

void StreamPlaylistItem::InitMetadata() {

  if (song_.title().isEmpty()) song_.set_title(song_.url().toString());
  if (song_.source() == Song::Source::Unknown) song_.set_source(Song::Source::Stream);
  if (song_.filetype() == Song::FileType::Unknown) song_.set_filetype(Song::FileType::Stream);
  song_.set_valid(true);

}

bool StreamPlaylistItem::InitFromQuery(const SqlRow &query) {

  song_.InitFromQuery(query, false, static_cast<int>(Song::kRowIdColumns.count()));
  InitMetadata();
  return true;

}

QVariant StreamPlaylistItem::DatabaseValue(const DatabaseColumn column) const {
  return PlaylistItem::DatabaseValue(column);
}

void StreamPlaylistItem::SetArtManual(const QUrl &cover_url) {

  song_.set_art_manual(cover_url);
  stream_song_.set_art_manual(cover_url);

}
