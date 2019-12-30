/*
 * Strawberry Music Player
 * This code was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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

#ifndef CONTEXTALBUMSMODEL_H
#define CONTEXTALBUMSMODEL_H

#include "config.h"


#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QIcon>
#include <QPixmap>
#include <QSettings>

#include "core/simpletreemodel.h"
#include "core/song.h"
#include "collection/collectionquery.h"
#include "collection/collectionitem.h"
#include "collection/sqlrow.h"
#include "covermanager/albumcoverloaderoptions.h"

class QMimeData;

class Application;
class CollectionBackend;
class CollectionItem;

class ContextAlbumsModel : public SimpleTreeModel<CollectionItem> {
  Q_OBJECT

 public:
  ContextAlbumsModel(CollectionBackend *backend, Application *app, QObject *parent = nullptr);
  ~ContextAlbumsModel();

  static const int kPrettyCoverSize;

  enum Role {
    Role_Type = Qt::UserRole + 1,
    Role_ContainerType,
    Role_SortText,
    Role_Key,
    Role_Artist,
    Role_Editable,
    LastRole
  };

  struct QueryResult {
    QueryResult() {}
    SqlRowList rows;
  };

  void GetChildSongs(CollectionItem *item, QList<QUrl> *urls, SongList *songs, QSet<int> *song_ids) const;
  SongList GetChildSongs(const QModelIndex &index) const;
  SongList GetChildSongs(const QModelIndexList &indexes) const;

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  Qt::ItemFlags flags(const QModelIndex &index) const;
  QStringList mimeTypes() const;
  QMimeData *mimeData(const QModelIndexList &indexes) const;
  bool canFetchMore(const QModelIndex &parent) const;

  static QString TextOrUnknown(const QString &text);
  static QString SortText(QString text);
  static QString SortTextForArtist(QString artist);
  static QString SortTextForSong(const Song &song);

  void Reset();
  void AddSongs(const SongList &songs);

 protected:
  void LazyPopulate(CollectionItem *item) { LazyPopulate(item, true); }
  void LazyPopulate(CollectionItem *item, bool signal);

 private slots:
  void AlbumCoverLoaded(const quint64 id, const QUrl &cover_url, const QImage &image);

 private:
  QueryResult RunQuery(CollectionItem *parent);
  void PostQuery(CollectionItem *parent, const QueryResult &result, bool signal);
  CollectionItem *ItemFromSong(CollectionItem::Type item_type, bool signal, CollectionItem *parent, const Song &s, int container_level);

  QString AlbumIconPixmapCacheKey(const QModelIndex &index) const;
  QVariant AlbumIcon(const QModelIndex &index);
  QVariant data(const CollectionItem *item, int role) const;
  bool CompareItems(const CollectionItem *a, const CollectionItem *b) const;

 private:
  CollectionBackend *backend_;
  Application *app_;
  QueryOptions query_options_;
  QMap<QString, CollectionItem*> container_nodes_;
  QMap<int, CollectionItem*> song_nodes_;
  QIcon album_icon_;
  QPixmap no_cover_icon_;
  QIcon playlists_dir_icon_;
  AlbumCoverLoaderOptions cover_loader_options_;
  typedef QPair<CollectionItem*, QString> ItemAndCacheKey;
  QMap<quint64, ItemAndCacheKey> pending_art_;
  QSet<QString> pending_cache_keys_;
};

#endif  // CONTEXTALBUMSMODEL_H
