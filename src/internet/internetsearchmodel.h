/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef INTERNETSEARCHMODEL_H
#define INTERNETSEARCHMODEL_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSet>
#include <QList>
#include <QMap>
#include <QString>
#include <QIcon>
#include <QPixmap>

#include "core/song.h"
#include "collection/collectionmodel.h"
#include "internetsearch.h"

class QMimeData;
class QSortFilterProxyModel;

class InternetSearchModel : public QStandardItemModel {
  Q_OBJECT

 public:
  InternetSearchModel(InternetSearch *engine, QObject *parent = nullptr);

  enum Role {
    Role_Result = CollectionModel::LastRole,
    Role_LazyLoadingArt,
    Role_ProviderIndex,
    LastRole
  };

  struct ContainerKey {
    int provider_index_;
    QString group_[3];
  };

  void set_proxy(QSortFilterProxyModel *proxy) { proxy_ = proxy; }
  void set_use_pretty_covers(bool pretty) { use_pretty_covers_ = pretty; }
  void SetGroupBy(const CollectionModel::Grouping &grouping, bool regroup_now);

  void Clear();

  InternetSearch::ResultList GetChildResults(const QModelIndexList &indexes) const;
  InternetSearch::ResultList GetChildResults(const QList<QStandardItem*> &items) const;

  QMimeData *mimeData(const QModelIndexList &indexes) const;

 public slots:
  void AddResults(const InternetSearch::ResultList &results);

 private:
  QStandardItem *BuildContainers(const Song &metadata, QStandardItem *parent, ContainerKey *key, int level = 0);
  void GetChildResults(const QStandardItem *item, InternetSearch::ResultList *results, QSet<const QStandardItem*> *visited) const;

 private:
  InternetSearch *engine_;
  QSortFilterProxyModel *proxy_;
  bool use_pretty_covers_;
  QIcon artist_icon_;
  QIcon album_icon_;
  QPixmap no_cover_icon_;
  CollectionModel::Grouping group_by_;
  QMap<ContainerKey, QStandardItem*> containers_;

};

inline uint qHash(const InternetSearchModel::ContainerKey &key) {
  return qHash(key.provider_index_) ^ qHash(key.group_[0]) ^ qHash(key.group_[1]) ^ qHash(key.group_[2]);
}

inline bool operator<(const InternetSearchModel::ContainerKey &left, const InternetSearchModel::ContainerKey &right) {
#define CMP(field)                           \
  if (left.field < right.field) return true; \
  if (left.field > right.field) return false

  CMP(provider_index_);
  CMP(group_[0]);
  CMP(group_[1]);
  CMP(group_[2]);
  return false;

#undef CMP
}

#endif  // INTERNETSEARCHMODEL_H
