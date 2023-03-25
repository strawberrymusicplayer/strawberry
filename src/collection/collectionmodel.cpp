/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>
#include <functional>
#include <algorithm>
#include <utility>
#include <optional>

#include <QObject>
#include <QtGlobal>
#include <QtConcurrent>
#include <QThread>
#include <QMutex>
#include <QFuture>
#include <QFutureWatcher>
#include <QDataStream>
#include <QMimeData>
#include <QIODevice>
#include <QList>
#include <QSet>
#include <QMap>
#include <QMetaType>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QChar>
#include <QRegularExpression>
#include <QPixmapCache>
#include <QNetworkDiskCache>
#include <QSettings>
#include <QStandardPaths>

#include "core/application.h"
#include "core/database.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/sqlrow.h"
#include "collectionfilteroptions.h"
#include "collectionquery.h"
#include "collectionqueryoptions.h"
#include "collectionbackend.h"
#include "collectiondirectorymodel.h"
#include "collectionitem.h"
#include "collectionmodel.h"
#include "playlist/playlistmanager.h"
#include "playlist/songmimedata.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"
#include "settings/collectionsettingspage.h"

const int CollectionModel::kPrettyCoverSize = 32;
const char *CollectionModel::kPixmapDiskCacheDir = "pixmapcache";

QNetworkDiskCache *CollectionModel::sIconCache = nullptr;

CollectionModel::CollectionModel(CollectionBackend *backend, Application *app, QObject *parent)
    : SimpleTreeModel<CollectionItem>(new CollectionItem(this), parent),
      backend_(backend),
      app_(app),
      dir_model_(new CollectionDirectoryModel(backend, this)),
      show_various_artists_(true),
      total_song_count_(0),
      total_artist_count_(0),
      total_album_count_(0),
      separate_albums_by_grouping_(false),
      artist_icon_(IconLoader::Load("folder-sound")),
      album_icon_(IconLoader::Load("cdcase")),
      init_task_id_(-1),
      use_pretty_covers_(true),
      show_dividers_(true),
      use_disk_cache_(false),
      use_lazy_loading_(true) {

  root_->lazy_loaded = true;

  group_by_[0] = GroupBy::AlbumArtist;
  group_by_[1] = GroupBy::AlbumDisc;
  group_by_[2] = GroupBy::None;

  cover_loader_options_.get_image_data_ = false;
  cover_loader_options_.get_image_ = true;
  cover_loader_options_.scale_output_image_ = true;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.desired_height_ = kPrettyCoverSize;

  if (app_) {
    QObject::connect(app_->album_cover_loader(), &AlbumCoverLoader::AlbumCoverLoaded, this, &CollectionModel::AlbumCoverLoaded);
  }

  QIcon nocover = IconLoader::Load("cdcase");
  if (!nocover.isNull()) {
    QList<QSize> nocover_sizes = nocover.availableSizes();
    no_cover_icon_ = nocover.pixmap(nocover_sizes.last()).scaled(kPrettyCoverSize, kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  if (app_ && !sIconCache) {
    sIconCache = new QNetworkDiskCache(this);
    sIconCache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + kPixmapDiskCacheDir);
    QObject::connect(app_, &Application::ClearPixmapDiskCache, this, &CollectionModel::ClearDiskCache);
  }

  QObject::connect(backend_, &CollectionBackend::SongsDiscovered, this, &CollectionModel::SongsDiscovered);
  QObject::connect(backend_, &CollectionBackend::SongsDeleted, this, &CollectionModel::SongsDeleted);
  QObject::connect(backend_, &CollectionBackend::DatabaseReset, this, &CollectionModel::Reset);
  QObject::connect(backend_, &CollectionBackend::TotalSongCountUpdated, this, &CollectionModel::TotalSongCountUpdatedSlot);
  QObject::connect(backend_, &CollectionBackend::TotalArtistCountUpdated, this, &CollectionModel::TotalArtistCountUpdatedSlot);
  QObject::connect(backend_, &CollectionBackend::TotalAlbumCountUpdated, this, &CollectionModel::TotalAlbumCountUpdatedSlot);
  QObject::connect(backend_, &CollectionBackend::SongsStatisticsChanged, this, &CollectionModel::SongsSlightlyChanged);
  QObject::connect(backend_, &CollectionBackend::SongsRatingChanged, this, &CollectionModel::SongsSlightlyChanged);

  backend_->UpdateTotalSongCountAsync();
  backend_->UpdateTotalArtistCountAsync();
  backend_->UpdateTotalAlbumCountAsync();

  ReloadSettings();

}

CollectionModel::~CollectionModel() {
  delete root_;
}

void CollectionModel::set_pretty_covers(const bool use_pretty_covers) {

  if (use_pretty_covers != use_pretty_covers_) {
    use_pretty_covers_ = use_pretty_covers;
    Reset();
  }
}

void CollectionModel::set_show_dividers(const bool show_dividers) {

  if (show_dividers != show_dividers_) {
    show_dividers_ = show_dividers;
    Reset();
  }

}

void CollectionModel::ReloadSettings() {

  QSettings s;

  s.beginGroup(CollectionSettingsPage::kSettingsGroup);

  use_disk_cache_ = s.value(CollectionSettingsPage::kSettingsDiskCacheEnable, false).toBool();

  QPixmapCache::setCacheLimit(static_cast<int>(MaximumCacheSize(&s, CollectionSettingsPage::kSettingsCacheSize, CollectionSettingsPage::kSettingsCacheSizeUnit, CollectionSettingsPage::kSettingsCacheSizeDefault) / 1024));

  if (sIconCache) {
    sIconCache->setMaximumCacheSize(MaximumCacheSize(&s, CollectionSettingsPage::kSettingsDiskCacheSize, CollectionSettingsPage::kSettingsDiskCacheSizeUnit, CollectionSettingsPage::kSettingsDiskCacheSizeDefault));
  }

  s.endGroup();

  if (!use_disk_cache_) {
    ClearDiskCache();
  }

}

void CollectionModel::Init(const bool async) {

  if (async) {
    // Show a loading indicator in the model.
    CollectionItem *loading = new CollectionItem(CollectionItem::Type_LoadingIndicator, root_);
    loading->display_text = tr("Loading...");
    loading->lazy_loaded = true;
    beginResetModel();
    endResetModel();

    // Show a loading indicator in the status bar too.
    if (app_) {
      init_task_id_ = app_->task_manager()->StartTask(tr("Loading songs"));
    }

    ResetAsync();
  }
  else {
    Reset();
  }

}

void CollectionModel::SongsDiscovered(const SongList &songs) {

  for (const Song &song : songs) {

    // Sanity check to make sure we don't add songs that are outside the user's filter
    if (!filter_options_.Matches(song)) continue;

    // Hey, we've already got that one!
    if (song_nodes_.contains(song.id())) continue;

    // Before we can add each song we need to make sure the required container items already exist in the tree.
    // These depend on which "group by" settings the user has on the collection.
    // Eg. if the user grouped by artist and album, we would need to make sure nodes for the song's artist and album were already in the tree.

    // Find parent containers in the tree
    CollectionItem *container = root_;
    QString key;
    for (int i = 0; i < 3; ++i) {
      GroupBy group_by = group_by_[i];
      if (group_by == GroupBy::None) break;

      if (!key.isEmpty()) key.append("-");

      // Special case: if the song is a compilation and the current GroupBy level is Artists, then we want the Various Artists node :(
      if (IsArtistGroupBy(group_by) && song.is_compilation()) {
        if (container->compilation_artist_node_ == nullptr) {
          CreateCompilationArtistNode(true, container);
        }
        container = container->compilation_artist_node_;
        key = container->key;
      }
      else {
        // Otherwise find the proper container at this level based on the item's key
        key.append(ContainerKey(group_by, separate_albums_by_grouping_, song));

        // Does it exist already?
        if (container_nodes_[i].contains(key)) {
          container = container_nodes_[i][key];
        }
        else {
          // Create the container
          container = ItemFromSong(group_by, separate_albums_by_grouping_, true, i == 0, container, song, i);
          container_nodes_[i].insert(key, container);
        }

      }

      // If we just created the damn thing then we don't need to continue into it any further because it'll get lazy-loaded properly later.
      if (!container->lazy_loaded && use_lazy_loading_) break;
    }
    if (!container->lazy_loaded && use_lazy_loading_) continue;

    // We've gone all the way down to the deepest level and everything was already lazy loaded, so now we have to create the song in the container.
    song_nodes_.insert(song.id(), ItemFromSong(GroupBy::None, separate_albums_by_grouping_, true, false, container, song, -1));
  }

}

void CollectionModel::SongsSlightlyChanged(const SongList &songs) {

  // This is called if there was a minor change to the songs that will not normally require the collection to be restructured.
  // We can just update our internal cache of Song objects without worrying about resetting the model.
  for (const Song &song : songs) {
    if (song_nodes_.contains(song.id())) {
      song_nodes_[song.id()]->metadata = song;
    }
  }

}

CollectionItem *CollectionModel::CreateCompilationArtistNode(const bool signal, CollectionItem *parent) {

  Q_ASSERT(parent->compilation_artist_node_ == nullptr);

  if (signal) beginInsertRows(ItemToIndex(parent), static_cast<int>(parent->children.count()), static_cast<int>(parent->children.count()));

  parent->compilation_artist_node_ = new CollectionItem(CollectionItem::Type_Container, parent);
  parent->compilation_artist_node_->compilation_artist_node_ = nullptr;
  if (parent != root_ && !parent->key.isEmpty()) parent->compilation_artist_node_->key.append(parent->key);
  parent->compilation_artist_node_->key.append(tr("Various artists"));
  parent->compilation_artist_node_->display_text = tr("Various artists");
  parent->compilation_artist_node_->sort_text = " various";
  parent->compilation_artist_node_->container_level = parent->container_level + 1;

  if (signal) endInsertRows();

  return parent->compilation_artist_node_;

}

QString CollectionModel::ContainerKey(const GroupBy group_by, const bool separate_albums_by_grouping, const Song &song) {

  QString key;

  switch (group_by) {
    case GroupBy::AlbumArtist:
      key = TextOrUnknown(song.effective_albumartist());
      break;
    case GroupBy::Artist:
      key = TextOrUnknown(song.artist());
      break;
    case GroupBy::Album:
      key = TextOrUnknown(song.album());
      if (!song.album_id().isEmpty()) key.append("-" + song.album_id());
      if (separate_albums_by_grouping && !song.grouping().isEmpty()) key.append("-" + song.grouping());
      break;
    case GroupBy::AlbumDisc:
      key = PrettyAlbumDisc(song.album(), song.disc());
      if (!song.album_id().isEmpty()) key.append("-" + song.album_id());
      if (separate_albums_by_grouping && !song.grouping().isEmpty()) key.append("-" + song.grouping());
      break;
    case GroupBy::YearAlbum:
      key = PrettyYearAlbum(song.year(), song.album());
      if (!song.album_id().isEmpty()) key.append("-" + song.album_id());
      if (separate_albums_by_grouping && !song.grouping().isEmpty()) key.append("-" + song.grouping());
      break;
    case GroupBy::YearAlbumDisc:
      key = PrettyYearAlbumDisc(song.year(), song.album(), song.disc());
      if (!song.album_id().isEmpty()) key.append("-" + song.album_id());
      if (separate_albums_by_grouping && !song.grouping().isEmpty()) key.append("-" + song.grouping());
      break;
    case GroupBy::OriginalYearAlbum:
      key = PrettyYearAlbum(song.effective_originalyear(), song.album());
      if (!song.album_id().isEmpty()) key.append("-" + song.album_id());
      if (separate_albums_by_grouping && !song.grouping().isEmpty()) key.append("-" + song.grouping());
      break;
    case GroupBy::OriginalYearAlbumDisc:
      key = PrettyYearAlbumDisc(song.effective_originalyear(), song.album(), song.disc());
      if (!song.album_id().isEmpty()) key.append("-" + song.album_id());
      if (separate_albums_by_grouping && !song.grouping().isEmpty()) key.append("-" + song.grouping());
      break;
    case GroupBy::Disc:
      key = PrettyDisc(song.disc());
      break;
    case GroupBy::Year:
      key = QString::number(std::max(0, song.year()));
      break;
    case GroupBy::OriginalYear:
      key = QString::number(std::max(0, song.effective_originalyear()));
      break;
    case GroupBy::Genre:
      key = TextOrUnknown(song.genre());
      break;
    case GroupBy::Composer:
      key = TextOrUnknown(song.composer());
      break;
    case GroupBy::Performer:
      key = TextOrUnknown(song.performer());
      break;
    case GroupBy::Grouping:
      key = TextOrUnknown(song.grouping());
      break;
    case GroupBy::FileType:
      key = song.TextForFiletype();
      break;
    case GroupBy::Samplerate:
      key = QString::number(std::max(0, song.samplerate()));
      break;
    case GroupBy::Bitdepth:
      key = QString::number(std::max(0, song.bitdepth()));
      break;
    case GroupBy::Bitrate:
      key = QString::number(std::max(0, song.bitrate()));
      break;
    case GroupBy::Format:
      if (song.samplerate() <= 0) {
        key = song.TextForFiletype();
      }
      else {
        if (song.bitdepth() <= 0) {
          key = QString("%1 (%2)").arg(song.TextForFiletype(), QString::number(song.samplerate() / 1000.0, 'G', 5));
        }
        else {
          key = QString("%1 (%2/%3)").arg(song.TextForFiletype(), QString::number(song.samplerate() / 1000.0, 'G', 5)).arg(song.bitdepth());
        }
      }
      break;
    case GroupBy::None:
    case GroupBy::GroupByCount:
      qLog(Error) << "GroupBy::None";
      break;
  }

  return key;

}

QString CollectionModel::DividerKey(const GroupBy group_by, CollectionItem *item) {

  // Items which are to be grouped under the same divider must produce the same divider key.  This will only get called for top-level items.

  if (item->sort_text.isEmpty()) return QString();

  switch (group_by) {
    case GroupBy::AlbumArtist:
    case GroupBy::Artist:
    case GroupBy::Album:
    case GroupBy::AlbumDisc:
    case GroupBy::Composer:
    case GroupBy::Performer:
    case GroupBy::Grouping:
    case GroupBy::Disc:
    case GroupBy::Genre:
    case GroupBy::Format:
    case GroupBy::FileType: {
      QChar c = item->sort_text[0];
      if (c.isDigit()) return "0";
      if (c == ' ') return QString();
      if (c.decompositionTag() != QChar::NoDecomposition) {
        QString decomposition = c.decomposition();
        return QChar(decomposition[0]);
      }
      return c;
    }

    case GroupBy::Year:
    case GroupBy::OriginalYear:
      return SortTextForNumber(item->sort_text.toInt() / 10 * 10);

    case GroupBy::YearAlbum:
    case GroupBy::YearAlbumDisc:
      return SortTextForNumber(item->metadata.year());

    case GroupBy::OriginalYearAlbum:
    case GroupBy::OriginalYearAlbumDisc:
      return SortTextForNumber(item->metadata.effective_originalyear());

    case GroupBy::Samplerate:
      return SortTextForNumber(item->metadata.samplerate());

    case GroupBy::Bitdepth:
      return SortTextForNumber(item->metadata.bitdepth());

    case GroupBy::Bitrate:
      return SortTextForNumber(item->metadata.bitrate());

    case GroupBy::None:
    case GroupBy::GroupByCount:
      return QString();
  }
  qLog(Error) << "Unknown GroupBy" << group_by << "for item" << item->display_text;
  return QString();

}

QString CollectionModel::DividerDisplayText(const GroupBy group_by, const QString &key) {

  // Pretty display text for the dividers.

  switch (group_by) {
    case GroupBy::AlbumArtist:
    case GroupBy::Artist:
    case GroupBy::Album:
    case GroupBy::AlbumDisc:
    case GroupBy::Composer:
    case GroupBy::Performer:
    case GroupBy::Disc:
    case GroupBy::Grouping:
    case GroupBy::Genre:
    case GroupBy::FileType:
    case GroupBy::Format:
      if (key == "0") return "0-9";
      return key.toUpper();

    case GroupBy::YearAlbum:
    case GroupBy::YearAlbumDisc:
    case GroupBy::OriginalYearAlbum:
    case GroupBy::OriginalYearAlbumDisc:
      if (key == "0000") return tr("Unknown");
      return key.toUpper();

    case GroupBy::Year:
    case GroupBy::OriginalYear:
      if (key == "0000") return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy::Samplerate:
      if (key == "000") return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy::Bitdepth:
      if (key == "000") return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy::Bitrate:
      if (key == "000") return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy::None:
    case GroupBy::GroupByCount:
      break;
  }
  qLog(Error) << "Unknown GroupBy" << group_by << "for divider key" << key;
  return QString();

}

void CollectionModel::SongsDeleted(const SongList &songs) {

  // Delete the actual song nodes first, keeping track of each parent so we might check to see if they're empty later.
  QSet<CollectionItem*> parents;
  for (const Song &song : songs) {

    if (song_nodes_.contains(song.id())) {
      CollectionItem *node = song_nodes_[song.id()];

      if (node->parent != root_) parents << node->parent;

      beginRemoveRows(ItemToIndex(node->parent), node->row, node->row);
      node->parent->Delete(node->row);
      song_nodes_.remove(song.id());
      endRemoveRows();

    }
    else {
      // If we get here it means some of the songs we want to delete haven't been lazy-loaded yet.
      // This is bad, because it would mean that to clean up empty parents we would need to lazy-load them all individually to see if they're empty.
      // This can take a very long time, so better to just reset the model and be done with it.
      Reset();
      return;
    }
  }

  // Now delete empty parents
  QSet<QString> divider_keys;
  while (!parents.isEmpty()) {
    // Since we are going to remove elements from the container, we need a copy to iterate over.
    // If we iterate over the original, the behavior will be undefined.
    QSet<CollectionItem*> parents_copy = parents;
    for (CollectionItem *node : parents_copy) {
      parents.remove(node);
      if (node->children.count() != 0) continue;

      // Consider its parent for the next round
      if (node->parent != root_) parents << node->parent;

      // Maybe consider its divider node
      if (node->container_level == 0) {
        divider_keys << DividerKey(group_by_[0], node);
      }

      // Special case the Various Artists node
      if (IsCompilationArtistNode(node)) {
        node->parent->compilation_artist_node_ = nullptr;
      }
      else if (container_nodes_[node->container_level].contains(node->key)) {
        container_nodes_[node->container_level].remove(node->key);
      }

      // Remove from pixmap cache
      const QString cache_key = AlbumIconPixmapCacheKey(ItemToIndex(node));
      QPixmapCache::remove(cache_key);
      if (use_disk_cache_ && sIconCache) sIconCache->remove(QUrl(cache_key));
      if (pending_cache_keys_.contains(cache_key)) {
        pending_cache_keys_.remove(cache_key);
      }

      // Remove from pending art loading
      for (QMap<quint64, ItemAndCacheKey>::iterator it = pending_art_.begin(); it != pending_art_.end();) {
        if (it.value().first == node) {
          it = pending_art_.erase(it);  // clazy:exclude=strict-iterators
        }
        else {
          ++it;
        }
      }

      // It was empty - delete it
      beginRemoveRows(ItemToIndex(node->parent), node->row, node->row);
      node->parent->Delete(node->row);
      endRemoveRows();
    }
  }

  // Delete empty dividers
  for (const QString &divider_key : std::as_const(divider_keys)) {
    if (!divider_nodes_.contains(divider_key)) continue;

    // Look to see if there are any other items still under this divider
    QList<CollectionItem*> container_nodes = container_nodes_[0].values();
    if (std::any_of(container_nodes.begin(), container_nodes.end(), [this, divider_key](CollectionItem *node){ return DividerKey(group_by_[0], node) == divider_key; })) {
      continue;
    }

    // Remove the divider
    int row = divider_nodes_[divider_key]->row;
    beginRemoveRows(ItemToIndex(root_), row, row);
    root_->Delete(row);
    endRemoveRows();
    divider_nodes_.remove(divider_key);
  }

}

QString CollectionModel::AlbumIconPixmapCacheKey(const QModelIndex &idx) const {

  QStringList path;
  QModelIndex idx_copy(idx);
  while (idx_copy.isValid()) {
    path.prepend(idx_copy.data().toString());
    idx_copy = idx_copy.parent();
  }

  return Song::TextForSource(backend_->source()) + "/" + path.join("/");

}

QVariant CollectionModel::AlbumIcon(const QModelIndex &idx) {

  CollectionItem *item = IndexToItem(idx);
  if (!item) return no_cover_icon_;

  // Check the cache for a pixmap we already loaded.
  const QString cache_key = AlbumIconPixmapCacheKey(idx);

  QPixmap cached_pixmap;
  if (QPixmapCache::find(cache_key, &cached_pixmap)) {
    return cached_pixmap;
  }

  // Try to load it from the disk cache
  if (use_disk_cache_ && sIconCache) {
    std::unique_ptr<QIODevice> cache(sIconCache->data(QUrl(cache_key)));
    if (cache) {
      QImage cached_image;
      if (cached_image.load(cache.get(), "XPM")) {
        QPixmapCache::insert(cache_key, QPixmap::fromImage(cached_image));
        return QPixmap::fromImage(cached_image);
      }
    }
  }

  // Maybe we're loading a pixmap already?
  if (pending_cache_keys_.contains(cache_key)) {
    return no_cover_icon_;
  }

  // No art is cached and we're not loading it already.  Load art for the first song in the album.
  SongList songs = GetChildSongs(idx);
  if (!songs.isEmpty()) {
    const quint64 id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options_, songs.first());
    pending_art_[id] = ItemAndCacheKey(item, cache_key);
    pending_cache_keys_.insert(cache_key);
  }

  return no_cover_icon_;

}

void CollectionModel::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (!pending_art_.contains(id)) return;

  ItemAndCacheKey item_and_cache_key = pending_art_.take(id);
  CollectionItem *item = item_and_cache_key.first;
  if (!item) return;

  const QString &cache_key = item_and_cache_key.second;

  pending_cache_keys_.remove(cache_key);

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

  // If we have a valid cover not already in the disk cache
  if (use_disk_cache_ && sIconCache && result.success && !result.image_scaled.isNull()) {
    std::unique_ptr<QIODevice> cached_img(sIconCache->data(QUrl(cache_key)));
    if (!cached_img) {
      QNetworkCacheMetaData item_metadata;
      item_metadata.setSaveToDisk(true);
      item_metadata.setUrl(QUrl(cache_key));
      QIODevice *cache = sIconCache->prepare(item_metadata);
      if (cache) {
        result.image_scaled.save(cache, "XPM");
        sIconCache->insert(cache);
      }
    }
  }

  const QModelIndex idx = ItemToIndex(item);
  if (!idx.isValid()) return;

  emit dataChanged(idx, idx);

}

QVariant CollectionModel::data(const QModelIndex &idx, const int role) const {

  const CollectionItem *item = IndexToItem(idx);

  // Handle a special case for returning album artwork instead of a generic CD icon.
  // this is here instead of in the other data() function to let us use the
  // QModelIndex& version of GetChildSongs, which satisfies const-ness, instead
  // of the CollectionItem *version, which doesn't.
  if (use_pretty_covers_) {
    bool is_album_node = false;
    if (role == Qt::DecorationRole && item->type == CollectionItem::Type_Container) {
      GroupBy container_group_by = group_by_[item->container_level];
      is_album_node = IsAlbumGroupBy(container_group_by);
    }
    if (is_album_node) {
      // It has const behaviour some of the time - that's ok right?
      return const_cast<CollectionModel*>(this)->AlbumIcon(idx);
    }
  }

  return data(item, role);

}

QVariant CollectionModel::data(const CollectionItem *item, const int role) const {

  GroupBy container_group_by = item->type == CollectionItem::Type_Container ? group_by_[item->container_level] : GroupBy::None;

  switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
      return item->DisplayText();

    case Qt::DecorationRole:
      switch (item->type) {
        case CollectionItem::Type_Container:
          switch (container_group_by) {
            case GroupBy::Album:
            case GroupBy::AlbumDisc:
            case GroupBy::YearAlbum:
            case GroupBy::YearAlbumDisc:
            case GroupBy::OriginalYearAlbum:
            case GroupBy::OriginalYearAlbumDisc:
              return album_icon_;
            case GroupBy::Artist:
            case GroupBy::AlbumArtist:
              return artist_icon_;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;

    case Role_Type:
      return item->type;

    case Role_IsDivider:
      return item->type == CollectionItem::Type_Divider;

    case Role_ContainerType:
      return static_cast<int>(container_group_by);

    case Role_Key:
      return item->key;

    case Role_Artist:
      return item->metadata.artist();

    case Role_Editable:{
      if (!item->lazy_loaded) {
        const_cast<CollectionModel*>(this)->LazyPopulate(const_cast<CollectionItem*>(item), true);
      }

      if (item->type == CollectionItem::Type_Container) {
        // If we have even one non editable item as a child, we ourselves are not available for edit
        if (item->children.isEmpty()) {
          return false;
        }
        else if (std::any_of(item->children.begin(), item->children.end(), [this, role](CollectionItem *child) { return !data(child, role).toBool(); })) {
          return false;
        }
        else {
          return true;
        }
      }
      else if (item->type == CollectionItem::Type_Song) {
        return item->metadata.IsEditable();
      }
      else {
        return false;
      }
    }

    case Role_SortText:
      return item->SortText();
    default:
      return QVariant();
  }

  return QVariant();

}

bool CollectionModel::HasCompilations(const QSqlDatabase &db, const CollectionFilterOptions &filter_options, const CollectionQueryOptions &query_options) {

  CollectionQuery q(db, backend_->songs_table(), backend_->fts_table(), filter_options);
  q.SetColumnSpec(query_options.column_spec());
  for (const CollectionQueryOptions::Where &where_clauses : query_options.where_clauses()) {
    q.AddWhere(where_clauses.column, where_clauses.value, where_clauses.op);
  }
  q.AddCompilationRequirement(true);
  q.SetLimit(1);

  if (!q.Exec()) {
    backend_->ReportErrors(q);
    return false;
  }

  return q.Next();

}

CollectionQueryOptions CollectionModel::PrepareQuery(CollectionItem *parent) {

  // Information about what we want the children to be
  const int child_level = parent == root_ ? 0 : parent->container_level + 1;
  const GroupBy child_group_by = child_level >= 3 ? GroupBy::None : group_by_[child_level];

  CollectionQueryOptions query_options;

  // Initialize the query.  child_group_by says what type of thing we want (artists, songs, etc.)
  SetQueryColumnSpec(child_group_by, separate_albums_by_grouping_, &query_options);

  // Walk up through the item's parents adding filters as necessary
  for (CollectionItem *p = parent; p && p->type == CollectionItem::Type_Container; p = p->parent) {
    AddQueryWhere(group_by_[p->container_level], separate_albums_by_grouping_, p, &query_options);
  }

  // Artists GroupBy is special - we don't want compilation albums appearing
  if (show_various_artists_ && IsArtistGroupBy(child_group_by)) {
    query_options.set_query_have_compilations(true);
  }

  return query_options;

}

CollectionModel::QueryResult CollectionModel::RunQuery(const CollectionFilterOptions &filter_options, const CollectionQueryOptions &query_options) {

  QMutexLocker l(backend_->db()->Mutex());

  QueryResult result;
  {

    QSqlDatabase db(backend_->db()->Connect());
    // Add the special Various artists node
    if (query_options.query_have_compilations() && HasCompilations(db, filter_options, query_options)) {
      result.create_va = true;
    }

    CollectionQuery q(db, backend_->songs_table(), backend_->fts_table(), filter_options);
    q.SetColumnSpec(query_options.column_spec());
    for (const CollectionQueryOptions::Where &where_clauses : query_options.where_clauses()) {
      q.AddWhere(where_clauses.column, where_clauses.value, where_clauses.op);
    }

    if (result.create_va) {
      q.AddCompilationRequirement(false);
    }
    else if (query_options.compilation_requirement() != CollectionQueryOptions::CompilationRequirement::None) {
      q.AddCompilationRequirement(query_options.compilation_requirement() == CollectionQueryOptions::CompilationRequirement::On);
    }

    if (q.Exec()) {
      while (q.Next()) {
        result.rows << SqlRow(q);
      }
    }
    else {
      backend_->ReportErrors(q);
    }

  }

  if (QThread::currentThread() != thread() && QThread::currentThread() != backend_->thread()) {
    backend_->db()->Close();
  }

  return result;

}

void CollectionModel::PostQuery(CollectionItem *parent, const CollectionModel::QueryResult &result, const bool signal) {

  // Information about what we want the children to be
  int child_level = parent == root_ ? 0 : parent->container_level + 1;
  GroupBy child_group_by = child_level >= 3 ? GroupBy::None : group_by_[child_level];

  if (result.create_va && parent->compilation_artist_node_ == nullptr) {
    CreateCompilationArtistNode(signal, parent);
  }

  // Step through the results
  for (const SqlRow &row : result.rows) {
    // Create the item - it will get inserted into the model here
    CollectionItem *item = ItemFromQuery(child_group_by, separate_albums_by_grouping_, signal, child_level == 0, parent, row, child_level);

    // Save a pointer to it for later
    if (child_group_by == GroupBy::None) {
      song_nodes_.insert(item->metadata.id(), item);
    }
    else {
      container_nodes_[child_level].insert(item->key, item);
    }
  }

}

void CollectionModel::LazyPopulate(CollectionItem *parent, const bool signal) {

  if (parent->lazy_loaded) return;
  parent->lazy_loaded = true;

  CollectionQueryOptions query_options = PrepareQuery(parent);
  QueryResult result = RunQuery(filter_options_, query_options);
  PostQuery(parent, result, signal);

}

void CollectionModel::ResetAsync() {

  CollectionQueryOptions query_options = PrepareQuery(root_);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QFuture<CollectionModel::QueryResult> future = QtConcurrent::run(&CollectionModel::RunQuery, this, filter_options_, query_options);
#else
  QFuture<CollectionModel::QueryResult> future = QtConcurrent::run(this, &CollectionModel::RunQuery, filter_options_, query_options);
#endif
  QFutureWatcher<CollectionModel::QueryResult> *watcher = new QFutureWatcher<CollectionModel::QueryResult>();
  QObject::connect(watcher, &QFutureWatcher<CollectionModel::QueryResult>::finished, this, &CollectionModel::ResetAsyncQueryFinished);
  watcher->setFuture(future);

}

void CollectionModel::ResetAsyncQueryFinished() {

  QFutureWatcher<CollectionModel::QueryResult> *watcher = static_cast<QFutureWatcher<CollectionModel::QueryResult>*>(sender());
  const struct QueryResult result = watcher->result();
  watcher->deleteLater();

  BeginReset();
  root_->lazy_loaded = true;

  PostQuery(root_, result, false);

  if (init_task_id_ != -1) {
    if (app_) {
      app_->task_manager()->SetTaskFinished(init_task_id_);
    }
    init_task_id_ = -1;
  }

  endResetModel();

}

void CollectionModel::BeginReset() {

  beginResetModel();
  delete root_;
  song_nodes_.clear();
  container_nodes_[0].clear();
  container_nodes_[1].clear();
  container_nodes_[2].clear();
  divider_nodes_.clear();
  pending_art_.clear();
  pending_cache_keys_.clear();

  root_ = new CollectionItem(this);
  root_->compilation_artist_node_ = nullptr;
  root_->lazy_loaded = false;

}

void CollectionModel::Reset() {

  BeginReset();

  // Populate top level
  LazyPopulate(root_, false);

  endResetModel();

}

void CollectionModel::SetQueryColumnSpec(const GroupBy group_by, const bool separate_albums_by_grouping, CollectionQueryOptions *query_options) {

  // Say what group_by of thing we want to get back from the database.
  switch (group_by) {
    case GroupBy::AlbumArtist:
      query_options->set_column_spec("DISTINCT effective_albumartist");
      break;
    case GroupBy::Artist:
      query_options->set_column_spec("DISTINCT artist");
      break;
    case GroupBy::Album:{
      QString query("DISTINCT album, album_id");
      if (separate_albums_by_grouping) query.append(", grouping");
      query_options->set_column_spec(query);
      break;
    }
    case GroupBy::AlbumDisc:{
      QString query("DISTINCT album, album_id, disc");
      if (separate_albums_by_grouping) query.append(", grouping");
      query_options->set_column_spec(query);
      break;
    }
    case GroupBy::YearAlbum:{
      QString query("DISTINCT year, album, album_id");
      if (separate_albums_by_grouping) query.append(", grouping");
      query_options->set_column_spec(query);
      break;
    }
    case GroupBy::YearAlbumDisc:{
      QString query("DISTINCT year, album, album_id, disc");
      if (separate_albums_by_grouping) query.append(", grouping");
      query_options->set_column_spec(query);
      break;
    }
    case GroupBy::OriginalYearAlbum:{
      QString query("DISTINCT year, originalyear, album, album_id");
      if (separate_albums_by_grouping) query.append(", grouping");
      query_options->set_column_spec(query);
      break;
    }
    case GroupBy::OriginalYearAlbumDisc:{
      QString query("DISTINCT year, originalyear, album, album_id, disc");
      if (separate_albums_by_grouping) query.append(", grouping");
      query_options->set_column_spec(query);
      break;
    }
    case GroupBy::Disc:
      query_options->set_column_spec("DISTINCT disc");
      break;
    case GroupBy::Year:
      query_options->set_column_spec("DISTINCT year");
      break;
    case GroupBy::OriginalYear:
      query_options->set_column_spec("DISTINCT effective_originalyear");
      break;
    case GroupBy::Genre:
      query_options->set_column_spec("DISTINCT genre");
      break;
    case GroupBy::Composer:
      query_options->set_column_spec("DISTINCT composer");
      break;
    case GroupBy::Performer:
      query_options->set_column_spec("DISTINCT performer");
      break;
    case GroupBy::Grouping:
      query_options->set_column_spec("DISTINCT grouping");
      break;
    case GroupBy::FileType:
      query_options->set_column_spec("DISTINCT filetype");
      break;
    case GroupBy::Format:
      query_options->set_column_spec("DISTINCT filetype, samplerate, bitdepth");
      break;
    case GroupBy::Samplerate:
      query_options->set_column_spec("DISTINCT samplerate");
      break;
    case GroupBy::Bitdepth:
      query_options->set_column_spec("DISTINCT bitdepth");
      break;
    case GroupBy::Bitrate:
      query_options->set_column_spec("DISTINCT bitrate");
      break;
    case GroupBy::None:
    case GroupBy::GroupByCount:
      query_options->set_column_spec("%songs_table.ROWID, " + Song::kColumnSpec);
      break;
  }

}

void CollectionModel::AddQueryWhere(const GroupBy group_by, const bool separate_albums_by_grouping, CollectionItem *item, CollectionQueryOptions *query_options) {

  // Say how we want the query to be filtered.  This is done once for each parent going up the tree.

  switch (group_by) {
    case GroupBy::AlbumArtist:
      if (IsCompilationArtistNode(item)) {
        query_options->set_compilation_requirement(CollectionQueryOptions::CompilationRequirement::On);
      }
      else {
        // Don't duplicate compilations outside the Various artists node
        query_options->set_compilation_requirement(CollectionQueryOptions::CompilationRequirement::Off);
        query_options->AddWhere("effective_albumartist", item->metadata.effective_albumartist());
      }
      break;
    case GroupBy::Artist:
      if (IsCompilationArtistNode(item)) {
        query_options->set_compilation_requirement(CollectionQueryOptions::CompilationRequirement::On);
      }
      else {
        // Don't duplicate compilations outside the Various artists node
        query_options->set_compilation_requirement(CollectionQueryOptions::CompilationRequirement::Off);
        query_options->AddWhere("artist", item->metadata.artist());
      }
      break;
    case GroupBy::Album:
      query_options->AddWhere("album", item->metadata.album());
      query_options->AddWhere("album_id", item->metadata.album_id());
      if (separate_albums_by_grouping) query_options->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy::AlbumDisc:
      query_options->AddWhere("album", item->metadata.album());
      query_options->AddWhere("album_id", item->metadata.album_id());
      query_options->AddWhere("disc", item->metadata.disc());
      if (separate_albums_by_grouping) query_options->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy::YearAlbum:
      query_options->AddWhere("year", item->metadata.year());
      query_options->AddWhere("album", item->metadata.album());
      query_options->AddWhere("album_id", item->metadata.album_id());
      if (separate_albums_by_grouping) query_options->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy::YearAlbumDisc:
      query_options->AddWhere("year", item->metadata.year());
      query_options->AddWhere("album", item->metadata.album());
      query_options->AddWhere("album_id", item->metadata.album_id());
      query_options->AddWhere("disc", item->metadata.disc());
      if (separate_albums_by_grouping) query_options->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy::OriginalYearAlbum:
      query_options->AddWhere("year", item->metadata.year());
      query_options->AddWhere("originalyear", item->metadata.originalyear());
      query_options->AddWhere("album", item->metadata.album());
      query_options->AddWhere("album_id", item->metadata.album_id());
      if (separate_albums_by_grouping) query_options->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy::OriginalYearAlbumDisc:
      query_options->AddWhere("year", item->metadata.year());
      query_options->AddWhere("originalyear", item->metadata.originalyear());
      query_options->AddWhere("album", item->metadata.album());
      query_options->AddWhere("album_id", item->metadata.album_id());
      query_options->AddWhere("disc", item->metadata.disc());
      if (separate_albums_by_grouping) query_options->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy::Disc:
      query_options->AddWhere("disc", item->metadata.disc());
      break;
    case GroupBy::Year:
      query_options->AddWhere("year", item->metadata.year());
      break;
    case GroupBy::OriginalYear:
      query_options->AddWhere("effective_originalyear", item->metadata.effective_originalyear());
      break;
    case GroupBy::Genre:
      query_options->AddWhere("genre", item->metadata.genre());
      break;
    case GroupBy::Composer:
      query_options->AddWhere("composer", item->metadata.composer());
      break;
    case GroupBy::Performer:
      query_options->AddWhere("performer", item->metadata.performer());
      break;
    case GroupBy::Grouping:
      query_options->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy::FileType:
      query_options->AddWhere("filetype", static_cast<int>(item->metadata.filetype()));
      break;
    case GroupBy::Format:
      query_options->AddWhere("filetype", static_cast<int>(item->metadata.filetype()));
      query_options->AddWhere("samplerate", item->metadata.samplerate());
      query_options->AddWhere("bitdepth", item->metadata.bitdepth());
      break;
    case GroupBy::Samplerate:
      query_options->AddWhere("samplerate", item->metadata.samplerate());
      break;
    case GroupBy::Bitdepth:
      query_options->AddWhere("bitdepth", item->metadata.bitdepth());
      break;
    case GroupBy::Bitrate:
      query_options->AddWhere("bitrate", item->metadata.bitrate());
      break;
    case GroupBy::None:
    case GroupBy::GroupByCount:
      qLog(Error) << "Unknown GroupBy" << group_by << "used in filter";
      break;
  }

}

CollectionItem *CollectionModel::InitItem(const GroupBy group_by, const bool signal, CollectionItem *parent, const int container_level) {

  CollectionItem::Type item_type = group_by == GroupBy::None ? CollectionItem::Type_Song : CollectionItem::Type_Container;

  if (signal) beginInsertRows(ItemToIndex(parent), static_cast<int>(parent->children.count()), static_cast<int>(parent->children.count()));

  // Initialize the item depending on what type it's meant to be
  CollectionItem *item = new CollectionItem(item_type, parent);
  item->compilation_artist_node_ = nullptr;
  item->container_level = container_level;

  return item;

}

CollectionItem *CollectionModel::ItemFromQuery(const GroupBy group_by, const bool separate_albums_by_grouping, const bool signal, const bool create_divider, CollectionItem *parent, const SqlRow &row, const int container_level) {

  CollectionItem *item = InitItem(group_by, signal, parent, container_level);

  if (parent != root_ && !parent->key.isEmpty()) {
    item->key = parent->key + "-";
  }

  switch (group_by) {
    case GroupBy::AlbumArtist:{
      item->metadata.set_albumartist(row.value(0).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = TextOrUnknown(item->metadata.albumartist());
      item->sort_text = SortTextForArtist(item->metadata.albumartist());
      break;
    }
    case GroupBy::Artist:{
      item->metadata.set_artist(row.value(0).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = TextOrUnknown(item->metadata.artist());
      item->sort_text = SortTextForArtist(item->metadata.artist());
      break;
    }
    case GroupBy::Album:{
      item->metadata.set_album(row.value(0).toString());
      item->metadata.set_album_id(row.value(1).toString());
      item->metadata.set_grouping(row.value(2).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = TextOrUnknown(item->metadata.album());
      item->sort_text = SortTextForArtist(item->metadata.album());
      break;
    }
    case GroupBy::AlbumDisc:{
      item->metadata.set_album(row.value(0).toString());
      item->metadata.set_album_id(row.value(1).toString());
      item->metadata.set_disc(row.value(2).toInt());
      item->metadata.set_grouping(row.value(3).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = PrettyAlbumDisc(item->metadata.album(), item->metadata.disc());
      item->sort_text = item->metadata.album() + SortTextForNumber(std::max(0, item->metadata.disc()));
      break;
    }
    case GroupBy::YearAlbum:{
      item->metadata.set_year(row.value(0).toInt());
      item->metadata.set_album(row.value(1).toString());
      item->metadata.set_album_id(row.value(2).toString());
      item->metadata.set_grouping(row.value(3).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = PrettyYearAlbum(item->metadata.year(), item->metadata.album());
      item->sort_text = SortTextForNumber(std::max(0, item->metadata.year())) + item->metadata.grouping() + item->metadata.album();
      break;
    }
    case GroupBy::YearAlbumDisc:{
      item->metadata.set_year(row.value(0).toInt());
      item->metadata.set_album(row.value(1).toString());
      item->metadata.set_album_id(row.value(2).toString());
      item->metadata.set_disc(row.value(3).toInt());
      item->metadata.set_grouping(row.value(4).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = PrettyYearAlbumDisc(item->metadata.year(), item->metadata.album(), item->metadata.disc());
      item->sort_text = SortTextForNumber(std::max(0, item->metadata.year())) + item->metadata.album() + SortTextForNumber(std::max(0, item->metadata.disc()));
      break;
    }
    case GroupBy::OriginalYearAlbum:{
      item->metadata.set_year(row.value(0).toInt());
      item->metadata.set_originalyear(row.value(1).toInt());
      item->metadata.set_album(row.value(2).toString());
      item->metadata.set_album_id(row.value(3).toString());
      item->metadata.set_grouping(row.value(4).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = PrettyYearAlbum(item->metadata.effective_originalyear(), item->metadata.album());
      item->sort_text = SortTextForNumber(std::max(0, item->metadata.effective_originalyear())) + item->metadata.grouping() + item->metadata.album();
      break;
    }
    case GroupBy::OriginalYearAlbumDisc:{
      item->metadata.set_year(row.value(0).toInt());
      item->metadata.set_originalyear(row.value(1).toInt());
      item->metadata.set_album(row.value(2).toString());
      item->metadata.set_album_id(row.value(3).toString());
      item->metadata.set_disc(row.value(4).toInt());
      item->metadata.set_grouping(row.value(5).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = PrettyYearAlbumDisc(item->metadata.effective_originalyear(), item->metadata.album(), item->metadata.disc());
      item->sort_text = SortTextForNumber(std::max(0, item->metadata.effective_originalyear())) + item->metadata.album() + SortTextForNumber(std::max(0, item->metadata.disc()));
      break;
    }
    case GroupBy::Disc:{
      item->metadata.set_disc(row.value(0).toInt());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      const int disc = std::max(0, row.value(0).toInt());
      item->display_text = PrettyDisc(disc);
      item->sort_text = SortTextForNumber(disc);
      break;
    }
    case GroupBy::Year:{
      item->metadata.set_year(row.value(0).toInt());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      const int year = std::max(0, item->metadata.year());
      item->display_text = QString::number(year);
      item->sort_text = SortTextForNumber(year) + " ";
      break;
    }
    case GroupBy::OriginalYear:{
      item->metadata.set_originalyear(row.value(0).toInt());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      const int year = std::max(0, item->metadata.originalyear());
      item->display_text = QString::number(year);
      item->sort_text = SortTextForNumber(year) + " ";
      break;
    }
    case GroupBy::Genre:{
      item->metadata.set_genre(row.value(0).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = TextOrUnknown(item->metadata.genre());
      item->sort_text = SortTextForArtist(item->metadata.genre());
      break;
    }
    case GroupBy::Composer:{
      item->metadata.set_composer(row.value(0).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = TextOrUnknown(item->metadata.composer());
      item->sort_text = SortTextForArtist(item->metadata.composer());
      break;
    }
    case GroupBy::Performer:{
      item->metadata.set_performer(row.value(0).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = TextOrUnknown(item->metadata.performer());
      item->sort_text = SortTextForArtist(item->metadata.performer());
      break;
    }
    case GroupBy::Grouping:{
      item->metadata.set_grouping(row.value(0).toString());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = TextOrUnknown(item->metadata.grouping());
      item->sort_text = SortTextForArtist(item->metadata.grouping());
      break;
    }
    case GroupBy::FileType:{
      item->metadata.set_filetype(static_cast<Song::FileType>(row.value(0).toInt()));
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      item->display_text = item->metadata.TextForFiletype();
      item->sort_text = item->metadata.TextForFiletype();
      break;
    }
    case GroupBy::Format:{
      item->metadata.set_filetype(static_cast<Song::FileType>(row.value(0).toInt()));
      item->metadata.set_samplerate(row.value(1).toInt());
      item->metadata.set_bitdepth(row.value(2).toInt());
      QString key = ContainerKey(group_by, separate_albums_by_grouping, item->metadata);
      item->key.append(key);
      item->display_text = key;
      item->sort_text = key;
      break;
    }
    case GroupBy::Samplerate:{
      item->metadata.set_samplerate(row.value(0).toInt());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      const int samplerate = std::max(0, item->metadata.samplerate());
      item->display_text = QString::number(samplerate);
      item->sort_text = SortTextForNumber(samplerate) + " ";
      break;
    }
    case GroupBy::Bitdepth:{
      item->metadata.set_bitdepth(row.value(0).toInt());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      const int bitdepth = std::max(0, item->metadata.bitdepth());
      item->display_text = QString::number(bitdepth);
      item->sort_text = SortTextForNumber(bitdepth) + " ";
      break;
    }
    case GroupBy::Bitrate:{
      item->metadata.set_bitrate(row.value(0).toInt());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, item->metadata));
      const int bitrate = std::max(0, item->metadata.bitrate());
      item->display_text = QString::number(bitrate);
      item->sort_text = SortTextForNumber(bitrate) + " ";
      break;
    }
    case GroupBy::None:
    case GroupBy::GroupByCount:
      item->metadata.InitFromQuery(row, true);
      item->key.append(TextOrUnknown(item->metadata.title()));
      item->display_text = item->metadata.TitleWithCompilationArtist();
      if (item->container_level == 1 && !IsAlbumGroupBy(group_by_[0])) {
        item->sort_text = SortText(item->metadata.title());
      }
      else {
        item->sort_text = SortTextForSong(item->metadata);
      }
      break;
  }

  FinishItem(group_by, signal, create_divider, parent, item);

  return item;

}

CollectionItem *CollectionModel::ItemFromSong(const GroupBy group_by, const bool separate_albums_by_grouping, const bool signal, const bool create_divider, CollectionItem *parent, const Song &s, const int container_level) {

  CollectionItem *item = InitItem(group_by, signal, parent, container_level);

  if (parent != root_ && !parent->key.isEmpty()) {
    item->key = parent->key + "-";
  }

  switch (group_by) {
    case GroupBy::AlbumArtist:{
      item->metadata.set_albumartist(s.effective_albumartist());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = TextOrUnknown(s.effective_albumartist());
      item->sort_text = SortTextForArtist(s.effective_albumartist());
      break;
    }
    case GroupBy::Artist:{
      item->metadata.set_artist(s.artist());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = TextOrUnknown(s.artist());
      item->sort_text = SortTextForArtist(s.artist());
      break;
    }
    case GroupBy::Album:{
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_grouping(s.grouping());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = TextOrUnknown(s.album());
      item->sort_text = SortTextForArtist(s.album());
      break;
    }
    case GroupBy::AlbumDisc:{
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_disc(s.disc() <= 0 ? -1 : s.disc());
      item->metadata.set_grouping(s.grouping());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = PrettyAlbumDisc(s.album(), s.disc());
      item->sort_text = s.album() + SortTextForNumber(std::max(0, s.disc()));
      break;
    }
    case GroupBy::YearAlbum:{
      item->metadata.set_year(s.year() <= 0 ? -1 : s.year());
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_grouping(s.grouping());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = PrettyYearAlbum(s.year(), s.album());
      item->sort_text = SortTextForNumber(std::max(0, s.year())) + s.grouping() + s.album();
      break;
    }
    case GroupBy::YearAlbumDisc:{
      item->metadata.set_year(s.year() <= 0 ? -1 : s.year());
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_disc(s.disc() <= 0 ? -1 : s.disc());
      item->metadata.set_grouping(s.grouping());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = PrettyYearAlbumDisc(s.year(), s.album(), s.disc());
      item->sort_text = SortTextForNumber(std::max(0, s.year())) + s.album() + SortTextForNumber(std::max(0, s.disc()));
      break;
    }
    case GroupBy::OriginalYearAlbum:{
      item->metadata.set_year(s.year() <= 0 ? -1 : s.year());
      item->metadata.set_originalyear(s.originalyear() <= 0 ? -1 : s.originalyear());
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_grouping(s.grouping());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = PrettyYearAlbum(s.effective_originalyear(), s.album());
      item->sort_text = SortTextForNumber(std::max(0, s.effective_originalyear())) + s.grouping() + s.album();
      break;
    }
    case GroupBy::OriginalYearAlbumDisc:{
      item->metadata.set_year(s.year() <= 0 ? -1 : s.year());
      item->metadata.set_originalyear(s.originalyear() <= 0 ? -1 : s.originalyear());
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_disc(s.disc() <= 0 ? -1 : s.disc());
      item->metadata.set_grouping(s.grouping());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = PrettyYearAlbumDisc(s.effective_originalyear(), s.album(), s.disc());
      item->sort_text = SortTextForNumber(std::max(0, s.effective_originalyear())) + s.album() + SortTextForNumber(std::max(0, s.disc()));
      break;
    }
    case GroupBy::Disc:{
      item->metadata.set_disc(s.disc() <= 0 ? -1 : s.disc());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      const int disc = std::max(0, s.disc());
      item->display_text = PrettyDisc(disc);
      item->sort_text = SortTextForNumber(disc);
      break;
    }
    case GroupBy::Year:{
      item->metadata.set_year(s.year() <= 0 ? -1 : s.year());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      const int year = std::max(0, s.year());
      item->display_text = QString::number(year);
      item->sort_text = SortTextForNumber(year) + " ";
      break;
    }
    case GroupBy::OriginalYear:{
      item->metadata.set_originalyear(s.effective_originalyear() <= 0 ? -1 : s.effective_originalyear());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      const int year = std::max(0, s.effective_originalyear());
      item->display_text = QString::number(year);
      item->sort_text = SortTextForNumber(year) + " ";
      break;
    }
    case GroupBy::Genre:{
      item->metadata.set_genre(s.genre());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = TextOrUnknown(s.genre());
      item->sort_text = SortTextForArtist(s.genre());
      break;
    }
    case GroupBy::Composer:{
      item->metadata.set_composer(s.composer());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = TextOrUnknown(s.composer());
      item->sort_text = SortTextForArtist(s.composer());
      break;
    }
    case GroupBy::Performer:{
      item->metadata.set_performer(s.performer());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = TextOrUnknown(s.performer());
      item->sort_text = SortTextForArtist(s.performer());
      break;
    }
    case GroupBy::Grouping:{
      item->metadata.set_grouping(s.grouping());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = TextOrUnknown(s.grouping());
      item->sort_text = SortTextForArtist(s.grouping());
      break;
    }
    case GroupBy::FileType:{
      item->metadata.set_filetype(s.filetype());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      item->display_text = s.TextForFiletype();
      item->sort_text = s.TextForFiletype();
      break;
    }
    case GroupBy::Format:{
      item->metadata.set_filetype(s.filetype());
      item->metadata.set_samplerate(s.samplerate());
      item->metadata.set_bitdepth(s.bitdepth());
      QString key = ContainerKey(group_by, separate_albums_by_grouping, s);
      item->key.append(key);
      item->display_text = key;
      item->sort_text = key;
      break;
    }
    case GroupBy::Samplerate:{
      item->metadata.set_samplerate(s.samplerate());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      const int samplerate = std::max(0, s.samplerate());
      item->display_text = QString::number(samplerate);
      item->sort_text = SortTextForNumber(samplerate) + " ";
      break;
    }
    case GroupBy::Bitdepth:{
      item->metadata.set_bitdepth(s.bitdepth());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      const int bitdepth = std::max(0, s.bitdepth());
      item->display_text = QString::number(bitdepth);
      item->sort_text = SortTextForNumber(bitdepth) + " ";
      break;
    }
    case GroupBy::Bitrate:{
      item->metadata.set_bitrate(s.bitrate());
      item->key.append(ContainerKey(group_by, separate_albums_by_grouping, s));
      const int bitrate = std::max(0, s.bitrate());
      item->display_text = QString::number(bitrate);
      item->sort_text = SortTextForNumber(bitrate) + " ";
      break;
    }
    case GroupBy::None:
    case GroupBy::GroupByCount:{
      item->metadata = s;
      item->key.append(TextOrUnknown(s.title()));
      item->display_text = s.TitleWithCompilationArtist();
      if (item->container_level == 1 && !IsAlbumGroupBy(group_by_[0])) {
        item->sort_text = SortText(s.title());
      }
      else {
        item->sort_text = SortTextForSong(s);
      }
      break;
    }
  }

  FinishItem(group_by, signal, create_divider, parent, item);
  if (s.url().scheme() == "cdda") item->lazy_loaded = true;

  return item;

}

void CollectionModel::FinishItem(const GroupBy group_by, const bool signal, const bool create_divider, CollectionItem *parent, CollectionItem *item) {

  if (group_by == GroupBy::None) item->lazy_loaded = true;

  if (signal) {
    endInsertRows();
  }

  // Create the divider entry if we're supposed to
  if (create_divider && show_dividers_) {
    QString divider_key = DividerKey(group_by, item);
    if (!divider_key.isEmpty()) {
      item->sort_text.prepend(divider_key + " ");
    }

    if (!divider_key.isEmpty() && !divider_nodes_.contains(divider_key)) {
      if (signal) {
        beginInsertRows(ItemToIndex(parent), static_cast<int>(parent->children.count()), static_cast<int>(parent->children.count()));
      }

      CollectionItem *divider = new CollectionItem(CollectionItem::Type_Divider, root_);
      divider->key = divider_key;
      divider->display_text = DividerDisplayText(group_by, divider_key);
      divider->sort_text = divider_key + "  ";
      divider->lazy_loaded = true;

      divider_nodes_[divider_key] = divider;

      if (signal) {
        endInsertRows();
      }
    }
  }

}

QString CollectionModel::TextOrUnknown(const QString &text) {

  if (text.isEmpty()) return tr("Unknown");
  return text;

}

QString CollectionModel::PrettyYearAlbum(const int year, const QString &album) {

  if (year <= 0) return TextOrUnknown(album);
  return QString::number(year) + " - " + TextOrUnknown(album);

}

QString CollectionModel::PrettyAlbumDisc(const QString &album, const int disc) {

  if (disc <= 0 || album.contains(Song::kAlbumRemoveDisc)) return TextOrUnknown(album);
  else return TextOrUnknown(album) + " - (Disc " + QString::number(disc) + ")";

}

QString CollectionModel::PrettyYearAlbumDisc(const int year, const QString &album, const int disc) {

  QString str;

  if (year <= 0) str = TextOrUnknown(album);
  else str = QString::number(year) + " - " + TextOrUnknown(album);

  if (!album.contains(Song::kAlbumRemoveDisc) && disc > 0) str += " - (Disc " + QString::number(disc) + ")";

  return str;

}

QString CollectionModel::PrettyDisc(const int disc) {

  return "Disc " + QString::number(std::max(1, disc));

}

QString CollectionModel::SortText(QString text) {

  if (text.isEmpty()) {
    text = " unknown";
  }
  else {
    text = text.toLower();
  }
  text = text.remove(QRegularExpression("[^\\w ]", QRegularExpression::UseUnicodePropertiesOption));

  return text;

}

QString CollectionModel::SortTextForArtist(QString artist) {

  artist = SortText(artist);

  for (const auto &i : Song::kArticles) {
    if (artist.startsWith(i)) {
      qint64 ilen = i.length();
      artist = artist.right(artist.length() - ilen) + ", " + i.left(ilen - 1);
      break;
    }
  }

  return artist;

}

QString CollectionModel::SortTextForNumber(const int number) {

  return QString("%1").arg(number, 4, 10, QChar('0'));
}

QString CollectionModel::SortTextForYear(const int year) {

  QString str = QString::number(year);
  return QString("0").repeated(qMax(0, 4 - str.length())) + str;

}

QString CollectionModel::SortTextForBitrate(const int bitrate) {

  QString str = QString::number(bitrate);
  return QString("0").repeated(qMax(0, 3 - str.length())) + str;

}

QString CollectionModel::SortTextForSong(const Song &song) {

  QString ret = QString::number(std::max(0, song.disc()) * 1000 + std::max(0, song.track()));
  ret.prepend(QString("0").repeated(6 - ret.length()));
  ret.append(song.url().toString());
  return ret;

}

Qt::ItemFlags CollectionModel::flags(const QModelIndex &idx) const {

  switch (IndexToItem(idx)->type) {
    case CollectionItem::Type_Song:
    case CollectionItem::Type_Container:
      return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    case CollectionItem::Type_Divider:
    case CollectionItem::Type_Root:
    case CollectionItem::Type_LoadingIndicator:
    default:
      return Qt::ItemIsEnabled;
  }

}

QStringList CollectionModel::mimeTypes() const {
  return QStringList() << "text/uri-list";
}

QMimeData *CollectionModel::mimeData(const QModelIndexList &indexes) const {

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

bool CollectionModel::CompareItems(const CollectionItem *a, const CollectionItem *b) const {

  QVariant left(data(a, CollectionModel::Role_SortText));
  QVariant right(data(b, CollectionModel::Role_SortText));

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  if (left.metaType().id() == QMetaType::Int)
#else
  if (left.type() == QVariant::Int)
#endif
    return left.toInt() < right.toInt();
  else
    return left.toString() < right.toString();

}

qint64 CollectionModel::MaximumCacheSize(QSettings *s, const char *size_id, const char *size_unit_id, const qint64 cache_size_default) {

  qint64 size = s->value(size_id, cache_size_default).toInt();
  int unit = s->value(size_unit_id, static_cast<int>(CollectionSettingsPage::CacheSizeUnit::MB)).toInt() + 1;

  do {
    size *= 1024;
    unit -= 1;
  } while (unit > 0);

  return size;

}

void CollectionModel::GetChildSongs(CollectionItem *item, QList<QUrl> *urls, SongList *songs, QSet<int> *song_ids) const {

  switch (item->type) {
    case CollectionItem::Type_Container: {
      const_cast<CollectionModel*>(this)->LazyPopulate(item);

      QList<CollectionItem*> children = item->children;
      std::sort(children.begin(), children.end(), std::bind(&CollectionModel::CompareItems, this, std::placeholders::_1, std::placeholders::_2));

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

SongList CollectionModel::GetChildSongs(const QModelIndexList &indexes) const {

  QList<QUrl> dontcare;
  SongList ret;
  QSet<int> song_ids;

  for (const QModelIndex &idx : indexes) {
    GetChildSongs(IndexToItem(idx), &dontcare, &ret, &song_ids);
  }
  return ret;

}

SongList CollectionModel::GetChildSongs(const QModelIndex &idx) const {
  return GetChildSongs(QModelIndexList() << idx);
}

void CollectionModel::SetFilterMode(CollectionFilterOptions::FilterMode filter_mode) {
  filter_options_.set_filter_mode(filter_mode);
  ResetAsync();
}

void CollectionModel::SetFilterAge(const int filter_age) {
  filter_options_.set_max_age(filter_age);
  ResetAsync();
}

void CollectionModel::SetFilterText(const QString &filter_text) {
  filter_options_.set_filter_text(filter_text);
  ResetAsync();
}

bool CollectionModel::canFetchMore(const QModelIndex &parent) const {

  if (!parent.isValid()) return false;

  CollectionItem *item = IndexToItem(parent);
  return !item->lazy_loaded;

}

void CollectionModel::SetGroupBy(const Grouping g, const std::optional<bool> separate_albums_by_grouping) {

  group_by_ = g;
  if (separate_albums_by_grouping) {
    separate_albums_by_grouping_ = separate_albums_by_grouping.value();
  }

  ResetAsync();
  emit GroupingChanged(g, separate_albums_by_grouping_);

}

const CollectionModel::GroupBy &CollectionModel::Grouping::operator[](const int i) const {

  switch (i) {
    case 0: return first;
    case 1: return second;
    case 2: return third;
    default: break;
  }
  qLog(Error) << "CollectionModel::Grouping[] index out of range" << i;
  return first;

}

CollectionModel::GroupBy &CollectionModel::Grouping::operator[](const int i) {

  switch (i) {
    case 0: return first;
    case 1: return second;
    case 2: return third;
    default: break;
  }
  qLog(Error) << "CollectionModel::Grouping[] index out of range" << i;

  return first;

}


void CollectionModel::TotalSongCountUpdatedSlot(const int count) {

  total_song_count_ = count;
  emit TotalSongCountUpdated(count);

}

void CollectionModel::TotalArtistCountUpdatedSlot(const int count) {

  total_artist_count_ = count;
  emit TotalArtistCountUpdated(count);

}

void CollectionModel::TotalAlbumCountUpdatedSlot(const int count) {

  total_album_count_ = count;
  emit TotalAlbumCountUpdated(count);

}

void CollectionModel::ClearDiskCache() {
  if (sIconCache) sIconCache->clear();
}

void CollectionModel::ExpandAll(CollectionItem *item) const {

  if (!item) item = root_;
  const_cast<CollectionModel*>(this)->LazyPopulate(const_cast<CollectionItem*>(item), false);
  for (CollectionItem *child : item->children) {
    ExpandAll(child);
  }

}

QDataStream &operator<<(QDataStream &s, const CollectionModel::Grouping g) {
  s << static_cast<quint32>(g.first) << static_cast<quint32>(g.second) << static_cast<quint32>(g.third);
  return s;
}

QDataStream &operator>>(QDataStream &s, CollectionModel::Grouping &g) {

  quint32 buf = 0;
  s >> buf;
  g.first = static_cast<CollectionModel::GroupBy>(buf);
  s >> buf;
  g.second = static_cast<CollectionModel::GroupBy>(buf);
  s >> buf;
  g.third = static_cast<CollectionModel::GroupBy>(buf);
  return s;

}
