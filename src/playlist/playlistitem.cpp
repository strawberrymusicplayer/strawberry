/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QtConcurrentRun>
#include <QFuture>
#include <QColor>

#include "core/sqlquery.h"
#include "core/song.h"

#include "collection/collectionplaylistitem.h"
#include "playlistitem.h"
#include "songplaylistitem.h"

#include "internet/internetplaylistitem.h"
#include "radios/radioplaylistitem.h"

using std::make_shared;

PlaylistItemPtr PlaylistItem::NewFromSource(const Song::Source source) {

  switch (source) {
    case Song::Source::Collection:
      return make_shared<CollectionPlaylistItem>();
    case Song::Source::Subsonic:
    case Song::Source::Tidal:
    case Song::Source::Qobuz:
      return make_shared<InternetPlaylistItem>(source);
    case Song::Source::Stream:
    case Song::Source::RadioParadise:
    case Song::Source::SomaFM:
      return make_shared<RadioPlaylistItem>(source);
    case Song::Source::LocalFile:
    case Song::Source::CDDA:
    case Song::Source::Device:
    case Song::Source::Unknown:
      break;
  }

  return make_shared<SongPlaylistItem>(source);

}

PlaylistItemPtr PlaylistItem::NewFromSong(const Song &song) {

  switch (song.source()) {
    case Song::Source::Collection:
      return make_shared<CollectionPlaylistItem>(song);
    case Song::Source::Subsonic:
    case Song::Source::Tidal:
    case Song::Source::Qobuz:
      return make_shared<InternetPlaylistItem>(song);
    case Song::Source::Stream:
    case Song::Source::RadioParadise:
    case Song::Source::SomaFM:
      return make_shared<RadioPlaylistItem>(song);
    case Song::Source::LocalFile:
    case Song::Source::CDDA:
    case Song::Source::Device:
    case Song::Source::Unknown:
      break;
  }

  return make_shared<SongPlaylistItem>(song);

}

PlaylistItem::~PlaylistItem() = default;

void PlaylistItem::BindToQuery(SqlQuery *query) const {

  query->BindValue(":type", static_cast<int>(source_));
  query->BindValue(":collection_id", DatabaseValue(Column_CollectionId));

  DatabaseSongMetadata().BindToQuery(query);

}

void PlaylistItem::SetTemporaryMetadata(const Song &metadata) {
  temp_metadata_ = metadata;
}

void PlaylistItem::UpdateTemporaryMetadata(const Song &metadata) {

  if (!temp_metadata_.is_valid()) return;

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

  if (background_colors_.isEmpty()) {
    return QColor();
  }
  else {
    QList<short> background_colors_keys = background_colors_.keys();
    return background_colors_[background_colors_keys.last()];
  }

}
bool PlaylistItem::HasCurrentBackgroundColor() const {
  return !background_colors_.isEmpty();
}

void PlaylistItem::SetForegroundColor(const short priority, const QColor &color) {
  foreground_colors_[priority] = color;
}
bool PlaylistItem::HasForegroundColor(const short priority) const {
  return foreground_colors_.contains(priority);
}
void PlaylistItem::RemoveForegroundColor(const short priority) {
  foreground_colors_.remove(priority);
}
QColor PlaylistItem::GetCurrentForegroundColor() const {

  if (foreground_colors_.isEmpty()) return QColor();
  else {
    QList<short> foreground_colors_keys = foreground_colors_.keys();
    return foreground_colors_[foreground_colors_keys.last()];
  }

}
bool PlaylistItem::HasCurrentForegroundColor() const {
  return !foreground_colors_.isEmpty();
}
void PlaylistItem::SetShouldSkip(const bool val) { should_skip_ = val; }
bool PlaylistItem::GetShouldSkip() const { return should_skip_; }
