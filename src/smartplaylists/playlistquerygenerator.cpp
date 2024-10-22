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

#include "config.h"

#include <QIODevice>
#include <QDataStream>
#include <QByteArray>
#include <QString>

#include "playlistquerygenerator.h"
#include "collection/collectionbackend.h"

PlaylistQueryGenerator::PlaylistQueryGenerator(QObject *parent) : PlaylistGenerator(parent), dynamic_(false), current_pos_(0) {}

PlaylistQueryGenerator::PlaylistQueryGenerator(const QString &name, const SmartPlaylistSearch &search, const bool dynamic, QObject *parent)
    : PlaylistGenerator(parent),
      search_(search),
      dynamic_(dynamic),
      current_pos_(0) {

  set_name(name);

}

void PlaylistQueryGenerator::Load(const SmartPlaylistSearch &search) {

  search_ = search;
  dynamic_ = false;
  current_pos_ = 0;

}

void PlaylistQueryGenerator::Load(const QByteArray &data) {

  QDataStream s(data);
  s >> search_;
  s >> dynamic_;

}

QByteArray PlaylistQueryGenerator::Save() const {

  QByteArray ret;
  QDataStream s(&ret, QIODevice::WriteOnly);
  s << search_;
  s << dynamic_;

  return ret;

}

PlaylistItemPtrList PlaylistQueryGenerator::Generate() {

  previous_ids_.clear();
  current_pos_ = 0;
  return GenerateMore(0);

}

PlaylistItemPtrList PlaylistQueryGenerator::GenerateMore(const int count) {

  SmartPlaylistSearch search_copy = search_;
  search_copy.id_not_in_ = previous_ids_;
  if (count > 0) {
    search_copy.limit_ = count;
  }

  if (search_copy.sort_type_ != SmartPlaylistSearch::SortType::Random) {
    search_copy.first_item_ = current_pos_;
    current_pos_ += search_copy.limit_;
  }

  const SongList songs = collection_backend_->ExecuteQuery(search_copy.ToSql(collection_backend_->songs_table()));
  PlaylistItemPtrList items;
  items.reserve(songs.count());
  for (const Song &song : songs) {
    items << PlaylistItem::NewFromSong(song);
    previous_ids_ << song.id();

    if (previous_ids_.count() > GetDynamicFuture() + GetDynamicHistory()) {
      previous_ids_.removeFirst();
    }
  }

  return items;

}
