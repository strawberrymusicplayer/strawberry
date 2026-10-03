/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QObject>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QMimeData>
#include <QSet>
#include <QString>

#include "core/mimedata.h"
#include "collection/collectionmodel.h"
#include "streamingsearchmodel.h"
#include "streamingsearchsortmodel.h"

StreamingSearchSortModel::StreamingSearchSortModel(QObject *parent) : QSortFilterProxyModel(parent) {}

bool StreamingSearchSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {

  // Dividers always go first. Handle the both-dividers case explicitly so the comparator stays a strict weak ordering (otherwise lessThan(a,b) and lessThan(b,a) would both be true).
  const bool left_is_divider = left.data(CollectionModel::Role_IsDivider).toBool();
  const bool right_is_divider = right.data(CollectionModel::Role_IsDivider).toBool();
  if (left_is_divider && !right_is_divider) return true;
  if (!left_is_divider && right_is_divider) return false;

  // Containers go before songs if they're at the same level
  const bool left_is_container = left.data(CollectionModel::Role_ContainerType).isValid();
  const bool right_is_container = right.data(CollectionModel::Role_ContainerType).isValid();
  if (left_is_container && !right_is_container) return true;
  if (right_is_container && !left_is_container) return false;

  // Containers get sorted on their sort text.
  if (left_is_container) {
    return QString::localeAwareCompare(left.data(CollectionModel::Role_SortText).toString(), right.data(CollectionModel::Role_SortText).toString()) < 0;
  }

  // Otherwise we're comparing songs.  Sort by disc, track, then title.
  const Song left_song = left.data(StreamingSearchModel::Role_Result).value<StreamingSearchModel::Result>().metadata_;
  const Song right_song = right.data(StreamingSearchModel::Role_Result).value<StreamingSearchModel::Result>().metadata_;

  if (left_song.disc() != right_song.disc()) return left_song.disc() < right_song.disc();
  if (left_song.track() != right_song.track()) return left_song.track() < right_song.track();

  return QString::localeAwareCompare(left_song.title(), right_song.title()) < 0;

}

StreamingSearchModel::ResultList StreamingSearchSortModel::GetChildResults(const QModelIndexList &proxy_indexes) const {

  StreamingSearchModel::ResultList results;
  QSet<QModelIndex> visited;
  for (const QModelIndex &proxy_index : proxy_indexes) {
    GetChildResults(proxy_index, &results, &visited);
  }

  return results;

}

void StreamingSearchSortModel::GetChildResults(const QModelIndex &proxy_index, StreamingSearchModel::ResultList *results, QSet<QModelIndex> *visited) const {

  if (!proxy_index.isValid() || visited->contains(proxy_index)) {
    return;
  }
  visited->insert(proxy_index);

  const int child_count = rowCount(proxy_index);
  if (child_count > 0) {
    // Visit the children through the proxy, so they are in the order they are shown.
    for (int row = 0; row < child_count; ++row) {
      GetChildResults(index(row, 0, proxy_index), results, visited);
    }
  }
  else {
    // A song, add its result.
    const QVariant result = proxy_index.data(StreamingSearchModel::Role_Result);
    if (result.isValid()) {
      results->append(result.value<StreamingSearchModel::Result>());
    }
  }

}

QMimeData *StreamingSearchSortModel::mimeData(const QModelIndexList &proxy_indexes) const {

  const StreamingSearchModel *search_model = qobject_cast<const StreamingSearchModel*>(sourceModel());
  if (!search_model) return nullptr;

  return search_model->LoadTracks(GetChildResults(proxy_indexes));

}
