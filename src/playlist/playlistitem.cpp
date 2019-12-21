/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <stdbool.h>

#include <QtConcurrentRun>
#include <QFuture>
#include <QString>
#include <QColor>
#include <QSqlQuery>
#include <QtDebug>

#include "core/logging.h"
#include "core/song.h"

#include "collection/collection.h"
#include "collection/collectionplaylistitem.h"
#include "playlistitem.h"
#include "songplaylistitem.h"

#include "internet/internetplaylistitem.h"

PlaylistItem::~PlaylistItem() {}

PlaylistItem *PlaylistItem::NewFromSource(const Song::Source &source) {

  switch (source) {
    case Song::Source_Collection:  return new CollectionPlaylistItem();
    case Song::Source_Tidal:
    case Song::Source_Stream:      return new InternetPlaylistItem(source);
    default:                       return new SongPlaylistItem(source);
  }

}

PlaylistItem *PlaylistItem::NewFromSongsTable(const QString &table, const Song &song) {

  if (table == SCollection::kSongsTable)
    return new CollectionPlaylistItem(song);

  qLog(Warning) << "Invalid PlaylistItem songs table:" << table;
  return nullptr;

}

void PlaylistItem::BindToQuery(QSqlQuery *query) const {

  query->bindValue(":type", source_);
  query->bindValue(":collection_id", DatabaseValue(Column_CollectionId));

  DatabaseSongMetadata().BindToQuery(query);

}

void PlaylistItem::SetTemporaryMetadata(const Song &metadata) {
  temp_metadata_ = metadata;
}

void PlaylistItem::UpdateTemporaryMetadata(const Song &metadata) {

  Song old_metadata = temp_metadata_;
  temp_metadata_ = metadata;

  // Keep samplerate, bitdepth and bitrate from the old metadata if it's not present in the new.
  if (temp_metadata_.samplerate() <= 0 && old_metadata.samplerate() > 0) temp_metadata_.set_samplerate(old_metadata.samplerate());
  if (temp_metadata_.bitdepth() <= 0 && old_metadata.bitdepth() > 0) temp_metadata_.set_bitdepth(old_metadata.bitdepth());
  if (temp_metadata_.bitrate() <= 0 && old_metadata.bitrate() > 0) temp_metadata_.set_bitrate(old_metadata.bitrate());

}

void PlaylistItem::ClearTemporaryMetadata() {
  temp_metadata_ = Song();
}

static void ReloadPlaylistItem(PlaylistItemPtr item) {
  item->Reload();
}

QFuture<void> PlaylistItem::BackgroundReload() {
  return QtConcurrent::run(ReloadPlaylistItem, shared_from_this());
}

void PlaylistItem::SetBackgroundColor(short priority, const QColor &color) {
  background_colors_[priority] = color;
}
bool PlaylistItem::HasBackgroundColor(short priority) const {
  return background_colors_.contains(priority);
}
void PlaylistItem::RemoveBackgroundColor(short priority) {
  background_colors_.remove(priority);
}
QColor PlaylistItem::GetCurrentBackgroundColor() const {
  return background_colors_.isEmpty() ? QColor() : background_colors_[background_colors_.keys().last()];
}
bool PlaylistItem::HasCurrentBackgroundColor() const {
  return !background_colors_.isEmpty();
}

void PlaylistItem::SetForegroundColor(short priority, const QColor &color) {
  foreground_colors_[priority] = color;
}
bool PlaylistItem::HasForegroundColor(short priority) const {
  return foreground_colors_.contains(priority);
}
void PlaylistItem::RemoveForegroundColor(short priority) {
  foreground_colors_.remove(priority);
}
QColor PlaylistItem::GetCurrentForegroundColor() const {
  return foreground_colors_.isEmpty() ? QColor() : foreground_colors_[foreground_colors_.keys().last()];
}
bool PlaylistItem::HasCurrentForegroundColor() const {
  return !foreground_colors_.isEmpty();
}
void PlaylistItem::SetShouldSkip(bool val) { should_skip_ = val; }
bool PlaylistItem::GetShouldSkip() const { return should_skip_; }

