/*
 * Strawberry Music Player
 * This code was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013-2020, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QVariant>
#include <QList>
#include <QSet>
#include <QRegExp>
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
#include "collection/collectionitem.h"
#include "collection/sqlrow.h"
#include "playlist/playlistmanager.h"
#include "playlist/songmimedata.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"

#include "contextalbumsmodel.h"

using std::placeholders::_1;
using std::placeholders::_2;

const int ContextAlbumsModel::kPrettyCoverSize = 32;

ContextAlbumsModel::ContextAlbumsModel(CollectionBackend *backend, Application *app, QObject *parent) :
      SimpleTreeModel<CollectionItem>(new CollectionItem(this), parent),
      backend_(backend),
      app_(app),
      album_icon_(IconLoader::Load("cdcase")),
      playlists_dir_icon_(IconLoader::Load("folder-sound")) {

  root_->lazy_loaded = true;

  cover_loader_options_.desired_height_ = kPrettyCoverSize;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.scale_output_image_ = true;

  connect(app_->album_cover_loader(), SIGNAL(AlbumCoverLoaded(quint64, AlbumCoverLoaderResult)), SLOT(AlbumCoverLoaded(quint64, AlbumCoverLoaderResult)));

  QIcon nocover = IconLoader::Load("cdcase");
  no_cover_icon_ = nocover.pixmap(nocover.availableSizes().last()).scaled(kPrettyCoverSize, kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

}

ContextAlbumsModel::~ContextAlbumsModel() { delete root_; }

void ContextAlbumsModel::AddSongs(const SongList &songs) {

  for (const Song &song : songs) {
    if (song_nodes_.contains(song.id())) continue;

    // Before we can add each song we need to make sure the required container items already exist in the tree.

    // Find parent containers in the tree
    CollectionItem *container = root_;

    // Does it exist already?
    if (!container_nodes_.contains(song.album())) {
      // Create the container
      container_nodes_[song.album()] = ItemFromSong(CollectionItem::Type_Container, true, container, song, 0);
    }
    container = container_nodes_[song.album()];
    if (!container->lazy_loaded) continue;

    // We've gone all the way down to the deepest level and everything was already lazy loaded, so now we have to create the song in the container.
    song_nodes_[song.id()] = ItemFromSong(CollectionItem::Type_Song, true, container, song, -1);
  }

}

QString ContextAlbumsModel::AlbumIconPixmapCacheKey(const QModelIndex &index) const {

  QStringList path;
  QModelIndex index_copy(index);
  while (index_copy.isValid()) {
    path.prepend(index_copy.data().toString());
    index_copy = index_copy.parent();
  }
  return "contextalbumsart:" + path.join("/");

}

QVariant ContextAlbumsModel::AlbumIcon(const QModelIndex &index) {

  CollectionItem *item = IndexToItem(index);
  if (!item) return no_cover_icon_;

  // Check the cache for a pixmap we already loaded.
  const QString cache_key = AlbumIconPixmapCacheKey(index);

  QPixmap cached_pixmap;
  if (QPixmapCache::find(cache_key, &cached_pixmap)) {
    return cached_pixmap;
  }

  // Maybe we're loading a pixmap already?
  if (pending_cache_keys_.contains(cache_key)) {
    return no_cover_icon_;
  }

  // No art is cached and we're not loading it already.  Load art for the first song in the album.
  SongList songs = GetChildSongs(index);
  if (!songs.isEmpty()) {
    const quint64 id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options_, songs.first());
    pending_art_[id] = ItemAndCacheKey(item, cache_key);
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
  pending_cache_keys_.remove(cache_key);

  // Insert this image in the cache.
  if (result.image_scaled.isNull()) {
    // Set the no_cover image so we don't continually try to load art.
    QPixmapCache::insert(cache_key, no_cover_icon_);
  }
  else {
    QPixmap image_pixmap;
    image_pixmap = QPixmap::fromImage(result.image_scaled);
    QPixmapCache::insert(cache_key, image_pixmap);
  }

  const QModelIndex index = ItemToIndex(item);
  emit dataChanged(index, index);

}

QVariant ContextAlbumsModel::data(const QModelIndex &index, int role) const {

  const CollectionItem *item = IndexToItem(index);

  if (role == Qt::DecorationRole && item->type == CollectionItem::Type_Container && item->container_level == 0) {
    return const_cast<ContextAlbumsModel*>(this)->AlbumIcon(index);
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
        case CollectionItem::Type_PlaylistContainer:
          return playlists_dir_icon_;
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
      if (!item->lazy_loaded) {
        const_cast<ContextAlbumsModel*>(this)->LazyPopulate(const_cast<CollectionItem*>(item), true);
      }

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

ContextAlbumsModel::QueryResult ContextAlbumsModel::RunQuery(CollectionItem *parent) {

  QueryResult result;
  CollectionQuery q(query_options_);
  q.SetColumnSpec("%songs_table.ROWID, " + Song::kColumnSpec);

  // Walk up through the item's parents adding filters as necessary
  CollectionItem *p = parent;
  while (p && p->type == CollectionItem::Type_Container) {
    if (p->container_level == 0) {
      q.AddWhere("album", p->key);
    }
    p = p->parent;
  }

  // Execute the query
  QMutexLocker l(backend_->db()->Mutex());

  if (!backend_->ExecQuery(&q)) return result;

  while (q.Next()) {
    result.rows << SqlRow(q);
  }
  return result;

}

void ContextAlbumsModel::PostQuery(CollectionItem *parent, const ContextAlbumsModel::QueryResult &result, bool signal) {

  int child_level = (parent == root_ ? 0 : parent->container_level + 1);

  for (const SqlRow &row : result.rows) {

    CollectionItem::Type item_type = (parent == root_ ? CollectionItem::Type_Container : CollectionItem::Type_Song);

    if (signal) beginInsertRows(ItemToIndex(parent), parent->children.count(), parent->children.count());

    CollectionItem *item = new CollectionItem(item_type, parent);
    item->container_level = child_level;
    item->metadata.InitFromQuery(row, true);
    item->key = item->metadata.title();
    item->display_text = item->metadata.TitleWithCompilationArtist();
    item->sort_text = SortTextForSong(item->metadata);
    if (parent != root_) item->lazy_loaded = true;

    if (signal) endInsertRows();

    if (parent == root_) container_nodes_[item->key] = item;
    else song_nodes_[item->metadata.id()] = item;

  }

}

void ContextAlbumsModel::LazyPopulate(CollectionItem *parent, bool signal) {

  if (parent->lazy_loaded) return;
  parent->lazy_loaded = true;

  QueryResult result = RunQuery(parent);
  PostQuery(parent, result, signal);

}

void ContextAlbumsModel::Reset() {

  QMap<QString, CollectionItem*>::iterator i = container_nodes_.begin();
  while (i != container_nodes_.end()) {
    const QString cache_key = AlbumIconPixmapCacheKey(ItemToIndex(i.value()));
    QPixmapCache::remove(cache_key);
    ++i;
  }

  beginResetModel();
  delete root_;
  song_nodes_.clear();
  container_nodes_.clear();
  pending_art_.clear();
  pending_cache_keys_.clear();

  root_ = new CollectionItem(this);
  root_->lazy_loaded = false;
  endResetModel();

}

CollectionItem *ContextAlbumsModel::ItemFromSong(CollectionItem::Type item_type, bool signal, CollectionItem *parent, const Song &s, int container_level) {

  if (signal) beginInsertRows(ItemToIndex(parent), parent->children.count(), parent->children.count());

  CollectionItem *item = new CollectionItem(item_type, parent);
  item->container_level = container_level;

  if (item->key.isNull()) item->key = s.album();
  //if (item->key.isNull()) item->key = s.effective_albumartist();
  item->display_text = TextOrUnknown(item->key);
  item->sort_text = SortTextForArtist(item->key);

  if (item_type == CollectionItem::Type_Song) item->lazy_loaded = true;
  if (signal) endInsertRows();

  return item;

}

QString ContextAlbumsModel::TextOrUnknown(const QString &text) {

  if (text.isEmpty()) return tr("Unknown");
  return text;

}

QString ContextAlbumsModel::SortText(QString text) {

  if (text.isEmpty()) {
    text = " unknown";
  }
  else {
    text = text.toLower();
  }
  text = text.remove(QRegExp("[^\\w ]"));

  return text;

}

QString ContextAlbumsModel::SortTextForArtist(QString artist) {

  artist = SortText(artist);

  if (artist.startsWith("the ")) {
    artist = artist.right(artist.length() - 4) + ", the";
  }
  else if (artist.startsWith("a ")) {
    artist = artist.right(artist.length() - 2) + ", a";
  }
  else if (artist.startsWith("an ")) {
    artist = artist.right(artist.length() - 3) + ", an";
  }

  return artist;

}

QString ContextAlbumsModel::SortTextForSong(const Song &song) {

  QString ret = QString::number(qMax(0, song.disc()) * 1000 + qMax(0, song.track()));
  ret.prepend(QString("0").repeated(6 - ret.length()));
  ret.append(song.url().toString());
  return ret;

}

Qt::ItemFlags ContextAlbumsModel::flags(const QModelIndex &index) const {

  switch (IndexToItem(index)->type) {
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

  for (const QModelIndex &index : indexes) {
    GetChildSongs(IndexToItem(index), &urls, &data->songs, &song_ids);
  }

  data->setUrls(urls);
  data->name_for_new_playlist_ = PlaylistManager::GetNameForNewPlaylist(data->songs);

  return data;

}

bool ContextAlbumsModel::CompareItems(const CollectionItem *a, const CollectionItem *b) const {

  QVariant left(data(a, ContextAlbumsModel::Role_SortText));
  QVariant right(data(b, ContextAlbumsModel::Role_SortText));

  if (left.type() == QVariant::Int) return left.toInt() < right.toInt();
  return left.toString() < right.toString();

}

void ContextAlbumsModel::GetChildSongs(CollectionItem *item, QList<QUrl> *urls, SongList *songs, QSet<int> *song_ids) const {

  switch (item->type) {
    case CollectionItem::Type_Container: {
      const_cast<ContextAlbumsModel*>(this)->LazyPopulate(item);

      QList<CollectionItem*> children = item->children;
      std::sort(children.begin(), children.end(), std::bind(&ContextAlbumsModel::CompareItems, this, _1, _2));

      for (CollectionItem *child : children)
        GetChildSongs(child, urls, songs, song_ids);
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

  for (const QModelIndex &index : indexes) {
    GetChildSongs(IndexToItem(index), &dontcare, &ret, &song_ids);
  }
  return ret;

}

SongList ContextAlbumsModel::GetChildSongs(const QModelIndex &index) const {
  return GetChildSongs(QModelIndexList() << index);
}

bool ContextAlbumsModel::canFetchMore(const QModelIndex &parent) const {

  if (!parent.isValid()) return false;

  CollectionItem *item = IndexToItem(parent);
  return !item->lazy_loaded;

}
