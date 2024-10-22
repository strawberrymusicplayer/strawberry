/*
 * Strawberry Music Player
 * Copyright 2021-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <functional>

#include <QSet>
#include <QList>
#include <QString>
#include <QUrl>

#include "core/song.h"
#include "core/songmimedata.h"
#include "filterparser/filterparser.h"
#include "filterparser/filtertree.h"
#include "collectionbackend.h"
#include "collectionfilter.h"
#include "collectionmodel.h"
#include "collectionitem.h"

CollectionFilter::CollectionFilter(QObject *parent) : QSortFilterProxyModel(parent), query_hash_(0) {

  setSortLocaleAware(true);
  setDynamicSortFilter(true);
  setRecursiveFilteringEnabled(true);

}

bool CollectionFilter::filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const {

  CollectionModel *model = qobject_cast<CollectionModel*>(sourceModel());
  if (!model) return false;
  const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
  if (!idx.isValid()) return false;
  CollectionItem *item = model->IndexToItem(idx);
  if (!item) return false;

  if (filter_string_.isEmpty()) return true;

  if (item->type != CollectionItem::Type::Song) {
    return item->type == CollectionItem::Type::LoadingIndicator;
  }

  size_t hash = qHash(filter_string_);
  if (hash != query_hash_) {
    FilterParser p(filter_string_);
    filter_tree_.reset(p.parse());
    query_hash_ = hash;
  }

  return item->metadata.is_valid() && filter_tree_->accept(item->metadata);

}

void CollectionFilter::SetFilterString(const QString &filter_string) {

  filter_string_ = filter_string;
  setFilterFixedString(filter_string);

}

QMimeData *CollectionFilter::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  CollectionModel *collection_model = qobject_cast<CollectionModel*>(sourceModel());
  SongMimeData *data = new SongMimeData;
  data->backend = collection_model->backend();

  QSet<int> song_ids;
  QList<QUrl> urls;
  for (const QModelIndex &idx : indexes) {
    const QModelIndex source_index = mapToSource(idx);
    CollectionItem *item = collection_model->IndexToItem(source_index);
    GetChildSongs(item, song_ids, urls, data->songs);
  }

  data->setUrls(urls);
  data->name_for_new_playlist_ = Song::GetNameForNewPlaylist(data->songs);

  return data;

}

void CollectionFilter::GetChildSongs(CollectionItem *item, QSet<int> &song_ids, QList<QUrl> &urls, SongList &songs) const {

  CollectionModel *collection_model = qobject_cast<CollectionModel*>(sourceModel());

  switch (item->type) {
    case CollectionItem::Type::Container:{
      QList<CollectionItem*> children = item->children;
      std::sort(children.begin(), children.end(), std::bind(&CollectionModel::CompareItems, collection_model, std::placeholders::_1, std::placeholders::_2));
      for (CollectionItem *child : children) {
        GetChildSongs(child, song_ids, urls, songs);
      }
      break;
    }
    case CollectionItem::Type::Song:{
      const QModelIndex idx = collection_model->ItemToIndex(item);
      if (filterAcceptsRow(idx.row(), idx.parent())) {
        urls << item->metadata.url();
        if (!song_ids.contains(item->metadata.id())) {
          song_ids.insert(item->metadata.id());
          songs << item->metadata;
        }
      }
      break;
    }
    default:
      break;
  }

}
