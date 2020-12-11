/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QVariant>
#include <QUrl>

#include "collectionplaylistitem.h"
#include "core/tagreaderclient.h"

class SqlRow;

CollectionPlaylistItem::CollectionPlaylistItem() : PlaylistItem(Song::Source_Collection) {
  song_.set_source(Song::Source_Collection);
}

CollectionPlaylistItem::CollectionPlaylistItem(const Song &song) : PlaylistItem(Song::Source_Collection), song_(song) {
  song_.set_source(Song::Source_Collection);
}

QUrl CollectionPlaylistItem::Url() const { return song_.url(); }

void CollectionPlaylistItem::Reload() {
  TagReaderClient::Instance()->ReadFileBlocking(song_.url().toLocalFile(), &song_);
}

bool CollectionPlaylistItem::InitFromQuery(const SqlRow &query) {

  // Rows from the songs tables come first
  song_.InitFromQuery(query, true);
  song_.set_source(Song::Source_Collection);
  return song_.is_valid();

}

QVariant CollectionPlaylistItem::DatabaseValue(DatabaseColumn column) const {

  switch (column) {
    case Column_CollectionId: return song_.id();
    default: return PlaylistItem::DatabaseValue(column);
  }

}

Song CollectionPlaylistItem::Metadata() const {

  if (HasTemporaryMetadata()) return temp_metadata_;
  return song_;

}

void CollectionPlaylistItem::SetArtManual(const QUrl &cover_url) {

  song_.set_art_manual(cover_url);
  if (HasTemporaryMetadata()) temp_metadata_.set_art_manual(cover_url);

}
