/*
 * Strawberry Music Player
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

#ifndef STREAMINGSEARCHMODEL_H
#define STREAMINGSEARCHMODEL_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QList>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QPixmap>
#include <QMetaType>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "collection/collectionmodel.h"

class MimeData;
class StreamingService;

class StreamingSearchModel : public QStandardItemModel {
  Q_OBJECT

 public:
  explicit StreamingSearchModel(SharedPtr<StreamingService> service, QObject *parent = nullptr);

  enum Role {
    Role_Result = CollectionModel::LastRole,
    Role_LazyLoadingArt,
    LastRole
  };

  struct Result {
    Song metadata_;
    QString pixmap_cache_key_;
  };
  using ResultList = QList<Result>;

  void set_use_pretty_covers(const bool pretty) { use_pretty_covers_ = pretty; }
  void SetGroupBy(const CollectionModel::Grouping grouping, const bool regroup_now);

  // Rebuilds the containers for the results already shown, e.g. after a display setting changes.
  void ReloadResults();

  void Clear();

  // Creates a SongMimeData with one Song for each Result.
  MimeData *LoadTracks(const ResultList &results) const;

 public Q_SLOTS:
  void AddResults(const StreamingSearchModel::ResultList &results);

 private:
  QStandardItem *BuildContainers(const Song &song, QStandardItem *parent, QStringList *key, const int level = 0);
  QString AlbumQualifierSuffix(const Song &song) const;
  ResultList AllResults() const;

 private:
  SharedPtr<StreamingService> service_;
  bool use_pretty_covers_;
  QIcon artist_icon_;
  QIcon album_icon_;
  QPixmap no_cover_icon_;
  CollectionModel::Grouping group_by_;
  // The containers by the keys of the containers above them and their own key.
  QHash<QStringList, QStandardItem*> containers_;
};

Q_DECLARE_METATYPE(StreamingSearchModel::Result)
Q_DECLARE_METATYPE(StreamingSearchModel::ResultList)

#endif  // STREAMINGSEARCHMODEL_H
