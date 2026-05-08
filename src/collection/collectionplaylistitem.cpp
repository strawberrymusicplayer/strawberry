/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QUuid>

#include "core/logging.h"
#include "collectionplaylistitem.h"
#include "tagreader/tagreaderclient.h"

class SqlRow;

CollectionPlaylistItem::CollectionPlaylistItem(const Song::Source source, const QUuid &uuid) : PlaylistItem(source, uuid) {
  song_.set_source(source);
}

CollectionPlaylistItem::CollectionPlaylistItem(const Song &song) : PlaylistItem(song.source()), song_(song) {}

bool CollectionPlaylistItem::InitFromQuery(const SqlRow &query) {

  int col = 0;
  switch (source_) {
    case Song::Source::Collection:
      col = 0;
      break;
    default:
      col = static_cast<int>(Song::kRowIdColumns.count());
      break;
  }

  song_.InitFromQuery(query, true, col);

  return song_.is_valid();

}

Song CollectionPlaylistItem::Reload() {

  if (!song_.url().isLocalFile()) return Song();

  Song result = song_;
  const TagReaderResult tag_result = TagReaderClient::Instance()->ReadFileBlocking(result.url().toLocalFile(), &result);
  if (!tag_result.success()) {
    qLog(Error) << "Could not reload file" << result.url() << tag_result.error_string();
    return Song();
  }

  return result;

}

QVariant CollectionPlaylistItem::DatabaseValue(const DatabaseColumn database_column) const {

  switch (database_column) {
    case DatabaseColumn::CollectionId:
      return song_.id();
    default:
      return PlaylistItem::DatabaseValue(database_column);
  }

}

void CollectionPlaylistItem::SetArtManual(const QUrl &cover_url) {

  song_.set_art_manual(cover_url);
  if (HasStreamMetadata()) stream_song_.set_art_manual(cover_url);

}
