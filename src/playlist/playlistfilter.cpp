/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QString>

#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "filterparser/filterparser.h"
#include "filterparser/filtertreenop.h"
#include "playlistfilter.h"

PlaylistFilter::PlaylistFilter(QObject *parent)
    : QSortFilterProxyModel(parent),
      filter_tree_(new FilterTreeNop),
      query_hash_(0) {

  setDynamicSortFilter(true);

}

PlaylistFilter::~PlaylistFilter() = default;

void PlaylistFilter::sort(int column, Qt::SortOrder order) {
  // Pass this through to the Playlist, it does sorting itself
  sourceModel()->sort(column, order);
}

bool PlaylistFilter::filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const {

  Playlist *playlist = qobject_cast<Playlist*>(sourceModel());
  if (!playlist) return false;
  const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
  if (!idx.isValid()) return false;
  PlaylistItemPtr item = playlist->item_at(idx.row());
  if (!item) return false;

  if (filter_string_.isEmpty()) return true;

  size_t hash = qHash(filter_string_);
  if (hash != query_hash_) {
    FilterParser p(filter_string_);
    filter_tree_.reset(p.parse());
    query_hash_ = hash;
  }

  return filter_tree_->accept(item->EffectiveMetadata());

}

void PlaylistFilter::SetFilterString(const QString &filter_string) {

  filter_string_ = filter_string;
  setFilterFixedString(filter_string);

}
