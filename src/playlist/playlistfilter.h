/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTFILTER_H
#define PLAYLISTFILTER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMap>
#include <QSet>
#include <QScopedPointer>
#include <QString>
#include <QSortFilterProxyModel>

class FilterTree;

class PlaylistFilter : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  explicit PlaylistFilter(QObject *parent = nullptr);
  ~PlaylistFilter() override;

  // QAbstractItemModel
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

  // QSortFilterProxyModel
  // public so Playlist::NextVirtualIndex and friends can get at it
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

 private:
  // Mutable because they're modified from filterAcceptsRow() const
  mutable QScopedPointer<FilterTree> filter_tree_;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  mutable size_t query_hash_;
#else
  mutable uint query_hash_;
#endif

  QMap<QString, int> column_names_;
  QSet<int> numerical_columns_;
};

#endif  // PLAYLISTFILTER_H
