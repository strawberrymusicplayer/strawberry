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

#ifndef COLLECTIONFILTER_H
#define COLLECTIONFILTER_H

#include "config.h"

#include <QSortFilterProxyModel>
#include <QScopedPointer>
#include <QSet>
#include <QList>
#include <QUrl>
#include <QModelIndex>
#include <QModelIndexList>

#include "core/song.h"
#include "collectionfilteroptions.h"
#include "filterparser/filtertree.h"

class CollectionItem;

class CollectionFilter : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  explicit CollectionFilter(QObject *parent = nullptr);

  void SetFilterString(const QString &filter_string);
  QString filter_string() const { return filter_string_; }

  void SetFilterMode(const CollectionFilterOptions::FilterMode filter_mode);
  void SetFilterMaxAge(const int filter_max_age);
  void SetFilterMinRating(const float filter_min_rating);

  // Updates what depends on all the songs, like which songs are duplicates, when songs were added or removed, or data used by the filters was changed.
  void SongsChanged();

  // Returns the songs below the indexes of this model which are not hidden by the filter.
  SongList GetChildSongs(const QModelIndexList &indexes) const;

 protected:
  bool filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;

 private:
  void GetChildSongs(CollectionItem *item, QSet<int> &song_ids, QList<QUrl> &urls, SongList &songs) const;
  bool HasSongFilter() const;
  bool AcceptsSong(const Song &song, const QSet<int> &duplicate_song_ids) const;
  void CalculateFilterData(QSet<int> *duplicate_song_ids, QSet<QString> *divider_keys) const;
  void BeginRowsFilterChange();
  void EndRowsFilterChange();

 private:
  QScopedPointer<FilterTree> filter_tree_;
  QString filter_string_;
  CollectionFilterOptions filter_options_;
  QSet<int> duplicate_song_ids_;
  QSet<QString> visible_divider_keys_;
};

#endif  // COLLECTIONFILTER_H
