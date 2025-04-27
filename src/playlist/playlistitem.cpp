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

#include <memory>

#include <QtConcurrentRun>
#include <QFuture>
#include <QColor>

#include "core/sqlquery.h"
#include "core/song.h"

#include "playlistitem.h"
#include "songplaylistitem.h"
#include "collection/collectionplaylistitem.h"
#include "streaming/streamserviceplaylistitem.h"
#include "radios/radiostreamplaylistitem.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

PlaylistItem::PlaylistItem(const Song::Source source) : should_skip_(false), source_(source) {}

PlaylistItem::~PlaylistItem() = default;

PlaylistItemPtr PlaylistItem::NewFromSource(const Song::Source source) {

  switch (source) {
    case Song::Source::Collection:
      return make_shared<CollectionPlaylistItem>(source);
    case Song::Source::Subsonic:
    case Song::Source::Tidal:
    case Song::Source::Spotify:
    case Song::Source::Qobuz:
      return make_shared<StreamServicePlaylistItem>(source);
    case Song::Source::Stream:
    case Song::Source::RadioParadise:
    case Song::Source::SomaFM:
      return make_shared<RadioStreamPlaylistItem>(source);
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
    case Song::Source::Spotify:
    case Song::Source::Qobuz:
      return make_shared<StreamServicePlaylistItem>(song);
    case Song::Source::Stream:
    case Song::Source::RadioParadise:
    case Song::Source::SomaFM:
      return make_shared<RadioStreamPlaylistItem>(song);
    case Song::Source::LocalFile:
    case Song::Source::CDDA:
    case Song::Source::Device:
    case Song::Source::Unknown:
      break;
  }

  return make_shared<SongPlaylistItem>(song);

}

void PlaylistItem::SetStreamMetadata(const Song &song) {
  stream_song_ = song;
}

void PlaylistItem::UpdateStreamMetadata(const Song &song) {

  if (!stream_song_.is_valid()) return;

  const Song old_stream_song = stream_song_;
  stream_song_ = song;

  // Keep samplerate, bitdepth and bitrate from the old metadata if it's not present in the new.
  if (stream_song_.samplerate() <= 0 && old_stream_song.samplerate() > 0) stream_song_.set_samplerate(old_stream_song.samplerate());
  if (stream_song_.bitdepth() <= 0 && old_stream_song.bitdepth() > 0) stream_song_.set_bitdepth(old_stream_song.bitdepth());
  if (stream_song_.bitrate() <= 0 && old_stream_song.bitrate() > 0) stream_song_.set_bitrate(old_stream_song.bitrate());

}

void PlaylistItem::ClearStreamMetadata() {
  stream_song_ = Song();
}

void PlaylistItem::BindToQuery(SqlQuery *query) const {

  query->BindValue(u":type"_s, static_cast<int>(source_));
  query->BindValue(u":collection_id"_s, DatabaseValue(DatabaseColumn::CollectionId));

  DatabaseSongMetadata().BindToQuery(query);

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

  QList<short> background_colors_keys = background_colors_.keys();
  return background_colors_[background_colors_keys.last()];

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

  QList<short> foreground_colors_keys = foreground_colors_.keys();
  return foreground_colors_[foreground_colors_keys.last()];

}

bool PlaylistItem::HasCurrentForegroundColor() const {
  return !foreground_colors_.isEmpty();
}

void PlaylistItem::SetShouldSkip(const bool should_skip) { should_skip_ = should_skip; }

bool PlaylistItem::GetShouldSkip() const { return should_skip_; }
