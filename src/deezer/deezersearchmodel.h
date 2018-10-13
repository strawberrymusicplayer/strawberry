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

#ifndef DEEZERSEARCHMODEL_H
#define DEEZERSEARCHMODEL_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMimeData>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSortFilterProxyModel>
#include <QMap>
#include <QSet>
#include <QList>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QPixmap>

#include "collection/collectionmodel.h"
#include "deezersearch.h"

class DeezerSearchModel : public QStandardItemModel {
  Q_OBJECT

 public:
  DeezerSearchModel(DeezerSearch *engine, QObject *parent = nullptr);

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

  DeezerSearch::ResultList GetChildResults(const QModelIndexList &indexes) const;
  DeezerSearch::ResultList GetChildResults(const QList<QStandardItem*> &items) const;

  QMimeData *mimeData(const QModelIndexList &indexes) const;

 public slots:
  void AddResults(const DeezerSearch::ResultList &results);

 private:
  QStandardItem *BuildContainers(const Song &metadata, QStandardItem *parent, ContainerKey *key, int level = 0);
  void GetChildResults(const QStandardItem *item, DeezerSearch::ResultList *results, QSet<const QStandardItem*> *visited) const;

 private:
  DeezerSearch *engine_;
  QSortFilterProxyModel *proxy_;
  bool use_pretty_covers_;
  QIcon artist_icon_;
  QPixmap no_cover_icon_;
  QIcon album_icon_;
  CollectionModel::Grouping group_by_;
  QMap<ContainerKey, QStandardItem*> containers_;

};

inline uint qHash(const DeezerSearchModel::ContainerKey &key) {
  return qHash(key.provider_index_) ^ qHash(key.group_[0]) ^ qHash(key.group_[1]) ^ qHash(key.group_[2]);
}

inline bool operator<(const DeezerSearchModel::ContainerKey &left, const DeezerSearchModel::ContainerKey &right) {
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

#endif  // DEEZERSEARCHMODEL_H
