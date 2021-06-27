/*
 * Strawberry Music Player
 * This code was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <functional>
#include <algorithm>

#include <QObject>
#include <QtGlobal>
#include <QMutex>
#include <QMimeData>
#include <QMetaType>
#include <QVariant>
#include <QList>
#include <QSet>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QPixmapCache>

#include "core/application.h"
#include "core/database.h"
#include "core/iconloader.h"
#include "collection/collectionquery.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionitem.h"
#include "collection/sqlrow.h"
#include "playlist/playlistmanager.h"
#include "playlist/songmimedata.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"

#include "contextalbumsmodel.h"

const int ContextAlbumsModel::kPrettyCoverSize = 32;

ContextAlbumsModel::ContextAlbumsModel(CollectionBackend *backend, Application *app, QObject *parent)
    : SimpleTreeModel<CollectionItem>(new CollectionItem(this), parent),
      backend_(backend),
      app_(app),
      album_icon_(IconLoader::Load("cdcase")) {

  cover_loader_options_.get_image_data_ = false;
  cover_loader_options_.scale_output_image_ = true;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.desired_height_ = kPrettyCoverSize;

  QObject::connect(app_->album_cover_loader(), &AlbumCoverLoader::AlbumCoverLoaded, this, &ContextAlbumsModel::AlbumCoverLoaded);

  QIcon nocover = IconLoader::Load("cdcase");
  QList<QSize> nocover_sizes = nocover.availableSizes();
  no_cover_icon_ = nocover.pixmap(nocover_sizes.last()).scaled(kPrettyCoverSize, kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

}

ContextAlbumsModel::~ContextAlbumsModel() { delete root_; }

void ContextAlbumsModel::AddSongs(const SongList &songs) {

  for (const Song &song : songs) {
    if (song_nodes_.contains(song.id())) continue;
    CollectionItem *container = root_;
    QString key = CollectionModel::ContainerKey(CollectionModel::GroupBy_Album, song);
    if (!container_nodes_.contains(key)) {
      container_nodes_.insert(key, ItemFromSong(CollectionItem::Type_Container, true, container, song, 0));
    }
    container = container_nodes_[key];
    song_nodes_[song.id()] = ItemFromSong(CollectionItem::Type_Song, true, container, song, -1);
  }

}

QString ContextAlbumsModel::AlbumIconPixmapCacheKey(const QModelIndex &idx) {

  QStringList path;
  QModelIndex index_copy(idx);
  while (index_copy.isValid()) {
    path.prepend(index_copy.data().toString());
    index_copy = index_copy.parent();
  }
  return "contextalbumsart:" + path.join("/");

}

QVariant ContextAlbumsModel::AlbumIcon(const QModelIndex &idx) {

  CollectionItem *item = IndexToItem(idx);
  if (!item) return no_cover_icon_;

  // Check the cache for a pixmap we already loaded.
  const QString cache_key = AlbumIconPixmapCacheKey(idx);

  QPixmap cached_pixmap;
  if (QPixmapCache::find(cache_key, &cached_pixmap)) {
    return cached_pixmap;
  }

  // Maybe we're loading a pixmap already?
  if (pending_cache_keys_.contains(cache_key)) {
    return no_cover_icon_;
  }

  // No art is cached and we're not loading it already.  Load art for the first song in the album.
  SongList songs = GetChildSongs(idx);
  if (!songs.isEmpty()) {
    const quint64 id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options_, songs.first());
    pending_art_.insert(id, ItemAndCacheKey(item, cache_key));
    pending_cache_keys_.insert(cache_key);
  }

  return no_cover_icon_;

}

void ContextAlbumsModel::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (!pending_art_.contains(id)) return;

  ItemAndCacheKey item_and_cache_key = pending_art_.take(id);

  CollectionItem *item = item_and_cache_key.first;
  if (!item) return;

  const QString &cache_key = item_and_cache_key.second;
  if (pending_cache_keys_.contains(cache_key)) {
    pending_cache_keys_.remove(cache_key);
  }

  // Insert this image in the cache.
  if (!result.success || result.image_scaled.isNull() || result.type == AlbumCoverLoaderResult::Type_ManuallyUnset) {
    // Set the no_cover image so we don't continually try to load art.
    QPixmapCache::insert(cache_key, no_cover_icon_);
  }
  else {
    QPixmap image_pixmap;
    image_pixmap = QPixmap::fromImage(result.image_scaled);
    QPixmapCache::insert(cache_key, image_pixmap);
  }

  const QModelIndex idx = ItemToIndex(item);

  emit dataChanged(idx, idx);

}

QVariant ContextAlbumsModel::data(const QModelIndex &idx, int role) const {

  const CollectionItem *item = IndexToItem(idx);

  if (role == Qt::DecorationRole && item->type == CollectionItem::Type_Container && item->container_level == 0) {
    return const_cast<ContextAlbumsModel*>(this)->AlbumIcon(idx);
  }

  return data(item, role);

}

QVariant ContextAlbumsModel::data(const CollectionItem *item, int role) const {

  switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
      return item->DisplayText();

    case Qt::DecorationRole:
      switch (item->type) {
        case CollectionItem::Type_Container:
          if (item->type == CollectionItem::Type_Container && item->container_level == 0) { return album_icon_; }
          break;
        default:
          break;
      }
      break;

    case Role_Type:
      return item->type;

    case Role_ContainerType:
      return item->type;

    case Role_Key:
      return item->key;

    case Role_Artist:
      return item->metadata.artist();

    case Role_Editable:
      if (item->type == CollectionItem::Type_Container) {
        // if we have even one non editable item as a child, we ourselves are not available for edit
        if (!item->children.isEmpty()) {
          for (CollectionItem *child : item->children) {
            if (!data(child, role).toBool()) {
              return false;
            }
          }
          return true;
        }
        else {
          return false;
        }
      }
      else if (item->type == CollectionItem::Type_Song) {
        return item->metadata.IsEditable();
      }
      else {
        return false;
      }

    case Role_SortText:
      return item->SortText();
  }
  return QVariant();

}

void ContextAlbumsModel::Reset() {

  for (QMap<QString, CollectionItem*>::iterator it = container_nodes_.begin() ; it != container_nodes_.end(); ++it) {
    const QString cache_key = AlbumIconPixmapCacheKey(ItemToIndex(it.value()));
    QPixmapCache::remove(cache_key);
  }

  beginResetModel();
  delete root_;
  song_nodes_.clear();
  container_nodes_.clear();
  pending_art_.clear();
  pending_cache_keys_.clear();

  root_ = new CollectionItem(this);
  root_->lazy_loaded = true;
  endResetModel();

}

CollectionItem *ContextAlbumsModel::ItemFromSong(CollectionItem::Type item_type, const bool signal, CollectionItem *parent, const Song &s, const int container_level) {

  if (signal) beginInsertRows(ItemToIndex(parent), parent->children.count(), parent->children.count());

  CollectionItem *item = new CollectionItem(item_type, parent);
  item->container_level = container_level;
  item->lazy_loaded = true;

  if (item_type == CollectionItem::Type_Container) {
    item->key = CollectionModel::ContainerKey(CollectionModel::GroupBy_Album, s);
    item->display_text = CollectionModel::TextOrUnknown(s.album());
    item->sort_text = CollectionModel::SortTextForArtist(s.album());
  }
  else {
    item->key = s.album() + " " + s.title();
    item->display_text = CollectionModel::TextOrUnknown(s.title());
    item->sort_text = CollectionModel::SortTextForSong(s);
    item->metadata = s;
  }

  if (signal) endInsertRows();

  return item;

}

Qt::ItemFlags ContextAlbumsModel::flags(const QModelIndex &idx) const {

  switch (IndexToItem(idx)->type) {
    case CollectionItem::Type_Song:
    case CollectionItem::Type_Container:
      return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    case CollectionItem::Type_Root:
    case CollectionItem::Type_LoadingIndicator:
    default:
      return Qt::ItemIsEnabled;
  }

}

QStringList ContextAlbumsModel::mimeTypes() const {
  return QStringList() << "text/uri-list";
}

QMimeData *ContextAlbumsModel::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  SongMimeData *data = new SongMimeData;
  QList<QUrl> urls;
  QSet<int> song_ids;

  data->backend = backend_;

  for (const QModelIndex &idx : indexes) {
    GetChildSongs(IndexToItem(idx), &urls, &data->songs, &song_ids);
  }

  data->setUrls(urls);
  data->name_for_new_playlist_ = PlaylistManager::GetNameForNewPlaylist(data->songs);

  return data;

}

bool ContextAlbumsModel::CompareItems(const CollectionItem *a, const CollectionItem *b) const {

  QVariant left(data(a, ContextAlbumsModel::Role_SortText));
  QVariant right(data(b, ContextAlbumsModel::Role_SortText));

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  if (left.metaType().id() == QMetaType::Int)
#else
  if (left.type() == QVariant::Int)
#endif
    return left.toInt() < right.toInt();
  else return left.toString() < right.toString();

}

void ContextAlbumsModel::GetChildSongs(CollectionItem *item, QList<QUrl> *urls, SongList *songs, QSet<int> *song_ids) const {

  switch (item->type) {
    case CollectionItem::Type_Container:{

      QList<CollectionItem*> children = item->children;
      std::sort(children.begin(), children.end(), std::bind(&ContextAlbumsModel::CompareItems, this, std::placeholders::_1, std::placeholders::_2));

      for (CollectionItem *child : children) {
        GetChildSongs(child, urls, songs, song_ids);
      }
      break;
    }

    case CollectionItem::Type_Song:
      urls->append(item->metadata.url());
      if (!song_ids->contains(item->metadata.id())) {
        songs->append(item->metadata);
        song_ids->insert(item->metadata.id());
      }
      break;

    default:
      break;
  }

}

SongList ContextAlbumsModel::GetChildSongs(const QModelIndexList &indexes) const {

  QList<QUrl> dontcare;
  SongList ret;
  QSet<int> song_ids;

  for (const QModelIndex &idx : indexes) {
    GetChildSongs(IndexToItem(idx), &dontcare, &ret, &song_ids);
  }
  return ret;

}

SongList ContextAlbumsModel::GetChildSongs(const QModelIndex &idx) const {
  return GetChildSongs(QModelIndexList() << idx);
}
