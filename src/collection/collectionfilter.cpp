/*
 * Strawberry Music Player
 * Copyright 2021-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <utility>

#include <QtGlobal>
#include <QHash>
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

CollectionFilter::CollectionFilter(QObject *parent) : QSortFilterProxyModel(parent) {

  setSortLocaleAware(true);
  setDynamicSortFilter(true);
  setRecursiveFilteringEnabled(true);

}

bool CollectionFilter::filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const {

  if (filter_string_.isEmpty() && !HasSongFilter()) return true;

  CollectionModel *model = qobject_cast<CollectionModel*>(sourceModel());
  if (!model) return false;
  const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
  if (!idx.isValid()) return false;
  CollectionItem *item = model->IndexToItem(idx);
  if (!item) return false;

  // Containers are shown when one of their songs is, by the recursive filtering.
  if (item->type == CollectionItem::Type::Divider) {
    return filter_string_.isEmpty() && visible_divider_keys_.contains(item->container_key);
  }
  if (item->type != CollectionItem::Type::Song) {
    return item->type == CollectionItem::Type::LoadingIndicator;
  }

  if (!item->metadata.is_valid() || !AcceptsSong(item->metadata, duplicate_song_ids_)) return false;

  return filter_string_.isEmpty() || (filter_tree_ && filter_tree_->accept(item->metadata));

}

bool CollectionFilter::HasSongFilter() const {

  return filter_options_.filter_mode() != CollectionFilterOptions::FilterMode::All || filter_options_.max_age() != -1 || filter_options_.min_rating() >= 0.0F;

}

bool CollectionFilter::AcceptsSong(const Song &song, const QSet<int> &duplicate_song_ids) const {

  if (filter_options_.filter_mode() == CollectionFilterOptions::FilterMode::Duplicates && !duplicate_song_ids.contains(song.id())) return false;

  return filter_options_.Matches(song);

}

void CollectionFilter::CalculateFilterData(QSet<int> *duplicate_song_ids, QSet<QString> *divider_keys) const {

  duplicate_song_ids->clear();
  divider_keys->clear();

  CollectionModel *model = qobject_cast<CollectionModel*>(sourceModel());
  if (!model || !HasSongFilter()) return;

  const QList<CollectionItem*> song_nodes = model->song_nodes();

  // Whether a song is a duplicate depends on the other songs, available songs are duplicates when they have the same artist, album and title, and none of these are empty.
  if (filter_options_.filter_mode() == CollectionFilterOptions::FilterMode::Duplicates) {
    QHash<QString, QList<int>> song_ids_by_key;
    for (const CollectionItem *node : song_nodes) {
      const Song &song = node->metadata;
      if (song.unavailable() || song.artist().isEmpty() || song.album().isEmpty() || song.title().isEmpty()) continue;
      song_ids_by_key[song.artist() + QChar(u'\0') + song.album() + QChar(u'\0') + song.title()] << song.id();
    }
    for (const QList<int> &song_ids : std::as_const(song_ids_by_key)) {
      if (song_ids.count() < 2) continue;
      for (const int song_id : song_ids) {
        duplicate_song_ids->insert(song_id);
      }
    }
  }

  // A divider is shown when one of the top level items below it has a song that is shown.
  for (const CollectionItem *node : song_nodes) {
    if (!AcceptsSong(node->metadata, *duplicate_song_ids)) continue;
    const CollectionItem *top_level_item = node;
    while (top_level_item->parent && top_level_item->parent->parent) {
      top_level_item = top_level_item->parent;
    }
    if (!top_level_item->divider_key.isEmpty()) {
      divider_keys->insert(top_level_item->divider_key);
    }
  }

}

void CollectionFilter::BeginRowsFilterChange() {

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  beginFilterChange();
#endif

}

void CollectionFilter::EndRowsFilterChange() {

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
  invalidateRowsFilter();
#endif

}

void CollectionFilter::SetFilterMode(const CollectionFilterOptions::FilterMode filter_mode) {

  if (filter_options_.filter_mode() == filter_mode) return;

  BeginRowsFilterChange();
  filter_options_.set_filter_mode(filter_mode);
  CalculateFilterData(&duplicate_song_ids_, &visible_divider_keys_);
  EndRowsFilterChange();

}

void CollectionFilter::SetFilterMaxAge(const int filter_max_age) {

  if (filter_options_.max_age() == filter_max_age) return;

  BeginRowsFilterChange();
  filter_options_.set_max_age(filter_max_age);
  CalculateFilterData(&duplicate_song_ids_, &visible_divider_keys_);
  EndRowsFilterChange();

}

void CollectionFilter::SetFilterMinRating(const float filter_min_rating) {

  if (filter_options_.min_rating() == filter_min_rating) return;

  BeginRowsFilterChange();
  filter_options_.set_min_rating(filter_min_rating);
  CalculateFilterData(&duplicate_song_ids_, &visible_divider_keys_);
  EndRowsFilterChange();

}

void CollectionFilter::SongsChanged() {

  if (!HasSongFilter()) return;

  QSet<int> duplicate_song_ids;
  QSet<QString> divider_keys;
  CalculateFilterData(&duplicate_song_ids, &divider_keys);
  if (duplicate_song_ids == duplicate_song_ids_ && divider_keys == visible_divider_keys_) return;

  BeginRowsFilterChange();
  duplicate_song_ids_ = duplicate_song_ids;
  visible_divider_keys_ = divider_keys;
  EndRowsFilterChange();

}

void CollectionFilter::SetFilterString(const QString &filter_string) {

  filter_string_ = filter_string;

  if (filter_string.isEmpty()) {
    filter_tree_.reset();
  }
  else {
    FilterParser p(filter_string);
    filter_tree_.reset(p.parse());
  }

  setFilterFixedString(filter_string);

}

QMimeData *CollectionFilter::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  CollectionModel *collection_model = qobject_cast<CollectionModel*>(sourceModel());
  if (!collection_model) return nullptr;
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

SongList CollectionFilter::GetChildSongs(const QModelIndexList &indexes) const {

  CollectionModel *collection_model = qobject_cast<CollectionModel*>(sourceModel());
  if (!collection_model) return SongList();

  QSet<int> song_ids;
  QList<QUrl> urls;
  SongList songs;
  for (const QModelIndex &idx : indexes) {
    GetChildSongs(collection_model->IndexToItem(mapToSource(idx)), song_ids, urls, songs);
  }

  return songs;

}

void CollectionFilter::GetChildSongs(CollectionItem *item, QSet<int> &song_ids, QList<QUrl> &urls, SongList &songs) const {

  CollectionModel *collection_model = qobject_cast<CollectionModel*>(sourceModel());
  if (!collection_model || !item) return;

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
