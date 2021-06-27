/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>

#include "collectionmodel.h"

class CollectionItem;

class CollectionFilter : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  explicit CollectionFilter(QObject *parent = nullptr);

 protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

 private:
  bool TagMatches(CollectionItem *item, const QMap<QString, QString> &tags) const;
  bool TagMatches(CollectionItem *item, const CollectionModel::GroupBy group_by, const QMap<QString, QString> &tags) const;
  bool ItemMatches(CollectionModel *model, CollectionItem *item, const QMap<QString, QString> &tags, const QString &filter) const;
  bool ChildrenMatches(CollectionModel *model, CollectionItem *item, const QMap<QString, QString> &tags, const QString &filter) const;
};

#endif  // COLLECTIONFILTER_H
