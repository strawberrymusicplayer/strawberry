/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include "internetsearchview.h"

class QMimeData;
class QSortFilterProxyModel;

class MimeData;
class InternetService;

class InternetSearchModel : public QStandardItemModel {
  Q_OBJECT

 public:
  explicit InternetSearchModel(InternetService *service, QObject *parent = nullptr);

  enum Role {
    Role_Result = CollectionModel::LastRole,
    Role_LazyLoadingArt,
    LastRole
  };

  struct ContainerKey {
    QString group_[3];
  };

  void set_proxy(QSortFilterProxyModel *proxy) { proxy_ = proxy; }
  void set_use_pretty_covers(const bool pretty) { use_pretty_covers_ = pretty; }
  void SetGroupBy(const CollectionModel::Grouping &grouping, const bool regroup_now);

  void Clear();

  InternetSearchView::ResultList GetChildResults(const QModelIndexList &indexes) const;
  InternetSearchView::ResultList GetChildResults(const QList<QStandardItem*> &items) const;

  QMimeData *mimeData(const QModelIndexList &indexes) const override;

  // Loads tracks for results that were previously emitted by ResultsAvailable.
  // The implementation creates a SongMimeData with one Song for each Result.
  MimeData *LoadTracks(const InternetSearchView::ResultList &results) const;

 public slots:
  void AddResults(const InternetSearchView::ResultList &results);

 private:
  QStandardItem *BuildContainers(const Song &s, QStandardItem *parent, ContainerKey *key, const int level = 0);
  void GetChildResults(const QStandardItem *item, InternetSearchView::ResultList *results, QSet<const QStandardItem*> *visited) const;

 private:
  InternetService *service_;
  QSortFilterProxyModel *proxy_;
  bool use_pretty_covers_;
  QIcon artist_icon_;
  QIcon album_icon_;
  QPixmap no_cover_icon_;
  CollectionModel::Grouping group_by_;
  QMap<ContainerKey, QStandardItem*> containers_;

};

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
inline size_t qHash(const InternetSearchModel::ContainerKey &key) {
#else
inline uint qHash(const InternetSearchModel::ContainerKey &key) {
#endif
  return qHash(key.group_[0]) ^ qHash(key.group_[1]) ^ qHash(key.group_[2]);
}

inline bool operator<(const InternetSearchModel::ContainerKey &left, const InternetSearchModel::ContainerKey &right) {
#define CMP(field)                           \
  if (left.field < right.field) return true; \
  if (left.field > right.field) return false

  CMP(group_[0]);
  CMP(group_[1]);
  CMP(group_[2]);
  return false;

#undef CMP
}

#endif  // INTERNETSEARCHMODEL_H
