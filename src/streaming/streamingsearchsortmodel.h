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

#ifndef STREAMINGSEARCHSORTMODEL_H
#define STREAMINGSEARCHSORTMODEL_H

#include <QSortFilterProxyModel>
#include <QModelIndex>
#include <QModelIndexList>
#include <QSet>

#include "streamingsearchmodel.h"

class QObject;
class QMimeData;

class StreamingSearchSortModel : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  explicit StreamingSearchSortModel(QObject *parent = nullptr);

  // Returns the results of the given indexes and the songs in them, in the order they are shown.
  StreamingSearchModel::ResultList GetChildResults(const QModelIndexList &proxy_indexes) const;

  QMimeData *mimeData(const QModelIndexList &proxy_indexes) const override;

 protected:
  bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

 private:
  void GetChildResults(const QModelIndex &proxy_index, StreamingSearchModel::ResultList *results, QSet<QModelIndex> *visited) const;
};

#endif  // STREAMINGSEARCHSORTMODEL_H
