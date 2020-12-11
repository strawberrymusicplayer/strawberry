/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QtGlobal>
#include <QtConcurrent>
#include <QThread>
#include <QMutex>
#include <QFuture>
#include <QDataStream>
#include <QMimeData>
#include <QIODevice>
#include <QList>
#include <QSet>
#include <QMap>
#include <QMetaType>
#include <QVariant>
#include <QByteArray>
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
#include <QtDebug>

#include "core/application.h"
#include "core/closure.h"
#include "core/database.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "core/taskmanager.h"
#include "collectionquery.h"
#include "collectionbackend.h"
#include "collectiondirectorymodel.h"
#include "collectionitem.h"
#include "collectionmodel.h"
#include "sqlrow.h"
#include "playlist/playlistmanager.h"
#include "playlist/songmimedata.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"
#include "settings/collectionsettingspage.h"

using std::placeholders::_1;
using std::placeholders::_2;

const char *CollectionModel::kSavedGroupingsSettingsGroup = "SavedGroupings";
const int CollectionModel::kPrettyCoverSize = 32;
const char *CollectionModel::kPixmapDiskCacheDir = "pixmapcache";

QNetworkDiskCache *CollectionModel::sIconCache = nullptr;

CollectionModel::CollectionModel(CollectionBackend *backend, Application *app, QObject *parent) :
      SimpleTreeModel<CollectionItem>(new CollectionItem(this), parent),
      backend_(backend),
      app_(app),
      dir_model_(new CollectionDirectoryModel(backend, this)),
      show_various_artists_(true),
      total_song_count_(0),
      total_artist_count_(0),
      total_album_count_(0),
      artist_icon_(IconLoader::Load("folder-sound")),
      album_icon_(IconLoader::Load("cdcase")),
      init_task_id_(-1),
      use_pretty_covers_(true),
      show_dividers_(true),
      use_disk_cache_(false),
      use_lazy_loading_(true) {

  root_->lazy_loaded = true;

  group_by_[0] = GroupBy_AlbumArtist;
  group_by_[1] = GroupBy_AlbumDisc;
  group_by_[2] = GroupBy_None;

  cover_loader_options_.desired_height_ = kPrettyCoverSize;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.scale_output_image_ = true;

  if (app_) {
    connect(app_->album_cover_loader(), SIGNAL(AlbumCoverLoaded(quint64, AlbumCoverLoaderResult)), SLOT(AlbumCoverLoaded(quint64, AlbumCoverLoaderResult)));
  }

  QIcon nocover = IconLoader::Load("cdcase");
  if (!nocover.isNull()) {
    no_cover_icon_ = nocover.pixmap(nocover.availableSizes().last()).scaled(kPrettyCoverSize, kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  if (app_ && !sIconCache) {
    sIconCache = new QNetworkDiskCache(this);
    sIconCache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + kPixmapDiskCacheDir);
    connect(app_, SIGNAL(ClearPixmapDiskCache()), SLOT(ClearDiskCache()));
  }

  connect(backend_, SIGNAL(SongsDiscovered(SongList)), SLOT(SongsDiscovered(SongList)));
  connect(backend_, SIGNAL(SongsDeleted(SongList)), SLOT(SongsDeleted(SongList)));
  connect(backend_, SIGNAL(DatabaseReset()), SLOT(Reset()));
  connect(backend_, SIGNAL(TotalSongCountUpdated(int)), SLOT(TotalSongCountUpdatedSlot(int)));
  connect(backend_, SIGNAL(TotalArtistCountUpdated(int)), SLOT(TotalArtistCountUpdatedSlot(int)));
  connect(backend_, SIGNAL(TotalAlbumCountUpdated(int)), SLOT(TotalAlbumCountUpdatedSlot(int)));
  connect(backend_, SIGNAL(SongsStatisticsChanged(SongList)), SLOT(SongsSlightlyChanged(SongList)));
  connect(backend_, SIGNAL(SongsRatingChanged(SongList)), SLOT(SongsSlightlyChanged(SongList)));

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

void CollectionModel::SaveGrouping(QString name) {

  qLog(Debug) << "Model, save to: " << name;

  QByteArray buffer;
  QDataStream ds(&buffer, QIODevice::WriteOnly);
  ds << group_by_;

  QSettings s;
  s.beginGroup(kSavedGroupingsSettingsGroup);
  s.setValue("version", "1");
  s.setValue(name, buffer);
  s.endGroup();

}

void CollectionModel::ReloadSettings() {

  QSettings s;

  s.beginGroup(CollectionSettingsPage::kSettingsGroup);

  use_disk_cache_ = s.value(CollectionSettingsPage::kSettingsDiskCacheEnable, false).toBool();

  QPixmapCache::setCacheLimit(MaximumCacheSize(&s, CollectionSettingsPage::kSettingsCacheSize, CollectionSettingsPage::kSettingsCacheSizeUnit, CollectionSettingsPage::kSettingsCacheSizeDefault) / 1024);

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
    if (app_)
      init_task_id_ = app_->task_manager()->StartTask(tr("Loading songs"));

    ResetAsync();
  }
  else {
    Reset();
  }

}

void CollectionModel::SongsDiscovered(const SongList &songs) {

  for (const Song &song : songs) {

    // Sanity check to make sure we don't add songs that are outside the user's filter
    if (!query_options_.Matches(song)) continue;

    // Hey, we've already got that one!
    if (song_nodes_.contains(song.id())) continue;

    // Before we can add each song we need to make sure the required container items already exist in the tree.
    // These depend on which "group by" settings the user has on the collection.
    // Eg. if the user grouped by artist and album, we would need to make sure nodes for the song's artist and album were already in the tree.

    // Find parent containers in the tree
    CollectionItem *container = root_;
    QString key;
    for (int i = 0; i < 3; ++i) {
      GroupBy type = group_by_[i];
      if (type == GroupBy_None) break;

      if (!key.isEmpty()) key.append("-");

      // Special case: if the song is a compilation and the current GroupBy level is Artists, then we want the Various Artists node :(
      if (IsArtistGroupBy(type) && song.is_compilation()) {
        if (container->compilation_artist_node_ == nullptr) {
          CreateCompilationArtistNode(true, container);
        }
        container = container->compilation_artist_node_;
        key = container->key;
      }
      else {
        // Otherwise find the proper container at this level based on the item's key
        switch (type) {
          case GroupBy_AlbumArtist:
            key.append(TextOrUnknown(song.effective_albumartist()));
            break;
          case GroupBy_Artist:
            key.append(TextOrUnknown(song.artist()));
            break;
          case GroupBy_Album:
            key.append(TextOrUnknown(song.album()));
            break;
          case GroupBy_AlbumDisc:
            key.append(PrettyAlbumDisc(song.album(), song.disc()));
            break;
          case GroupBy_YearAlbum:
            key.append(PrettyYearAlbum(song.year(), song.album()));
            break;
          case GroupBy_YearAlbumDisc:
            key.append(PrettyYearAlbumDisc(song.year(), song.album(), song.disc()));
            break;
          case GroupBy_OriginalYearAlbum:
            key.append(PrettyYearAlbum(song.effective_originalyear(), song.album()));
            break;
          case GroupBy_OriginalYearAlbumDisc:
            key.append(PrettyYearAlbumDisc(song.effective_originalyear(), song.album(), song.disc()));
            break;
          case GroupBy_Composer:
            key.append(TextOrUnknown(song.composer()));
            break;
          case GroupBy_Performer:
            key.append(TextOrUnknown(song.performer()));
            break;
          case GroupBy_Grouping:
            key.append(TextOrUnknown(song.grouping()));
            break;
          case GroupBy_Disc:
            key.append(PrettyDisc(song.disc()));
            break;
          case GroupBy_Genre:
            key.append(TextOrUnknown(song.genre()));
            break;
          case GroupBy_Year:
            key.append(QString::number(qMax(0, song.year())));
	    break;
          case GroupBy_OriginalYear:
            key.append(QString::number(qMax(0, song.effective_originalyear())));
            break;
          case GroupBy_FileType:
            key.append(song.TextForFiletype());
            break;
          case GroupBy_Samplerate:
            key.append(QString::number(qMax(0, song.samplerate())));
            break;
          case GroupBy_Bitdepth:
            key.append(QString::number(qMax(0, song.bitdepth())));
            break;
          case GroupBy_Bitrate:
            key.append(QString::number(qMax(0, song.bitrate())));
            break;
          case GroupBy_Format:
            if (song.samplerate() <= 0) {
              key.append(song.TextForFiletype());
            }
            else {
              if (song.bitdepth() <= 0) {
                key.append(QString("%1 (%2)").arg(song.TextForFiletype()).arg(QString::number(song.samplerate() / 1000.0, 'G', 5)));
              }
              else {
                key.append(QString("%1 (%2/%3)").arg(song.TextForFiletype()).arg(QString::number(song.samplerate() / 1000.0, 'G', 5)).arg(song.bitdepth()));
              }
            }
            break;
          case GroupBy_None:
          case GroupByCount:
            qLog(Error) << "GroupBy_None";
            break;
        }

        // Does it exist already?
        if (!container_nodes_[i].contains(key)) {
          // Create the container
          container_nodes_[i][key] = ItemFromSong(type, true, i == 0, container, song, i);
        }
        container = container_nodes_[i][key];

      }

      // If we just created the damn thing then we don't need to continue into it any further because it'll get lazy-loaded properly later.
      if (!container->lazy_loaded && use_lazy_loading_) break;
    }
    if (!container->lazy_loaded && use_lazy_loading_) continue;

    // We've gone all the way down to the deepest level and everything was already lazy loaded, so now we have to create the song in the container.
    song_nodes_[song.id()] = ItemFromSong(GroupBy_None, true, false, container, song, -1);
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

  if (signal) beginInsertRows(ItemToIndex(parent), parent->children.count(), parent->children.count());

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

QString CollectionModel::DividerKey(const GroupBy type, CollectionItem *item) const {

  // Items which are to be grouped under the same divider must produce the same divider key.  This will only get called for top-level items.

  if (item->sort_text.isEmpty()) return QString();

  switch (type) {
    case GroupBy_AlbumArtist:
    case GroupBy_Artist:
    case GroupBy_Album:
    case GroupBy_AlbumDisc:
    case GroupBy_Composer:
    case GroupBy_Performer:
    case GroupBy_Grouping:
    case GroupBy_Disc:
    case GroupBy_Genre:
    case GroupBy_Format:
    case GroupBy_FileType: {
      QChar c = item->sort_text[0];
      if (c.isDigit()) return "0";
      if (c == ' ') return QString();
      if (c.decompositionTag() != QChar::NoDecomposition) {
        return QChar(c.decomposition()[0]);
      }
      return c;
    }

    case GroupBy_Year:
    case GroupBy_OriginalYear:
      return SortTextForNumber(item->sort_text.toInt() / 10 * 10);

    case GroupBy_YearAlbum:
    case GroupBy_YearAlbumDisc:
      return SortTextForNumber(item->metadata.year());

    case GroupBy_OriginalYearAlbum:
    case GroupBy_OriginalYearAlbumDisc:
      return SortTextForNumber(item->metadata.effective_originalyear());

    case GroupBy_Samplerate:
      return SortTextForNumber(item->metadata.samplerate());

    case GroupBy_Bitdepth:
      return SortTextForNumber(item->metadata.bitdepth());

    case GroupBy_Bitrate:
      return SortTextForNumber(item->metadata.bitrate());

    case GroupBy_None:
    case GroupByCount:
      return QString();
  }
  qLog(Error) << "Unknown GroupBy type" << type << "for item" << item->display_text;
  return QString();

}

QString CollectionModel::DividerDisplayText(const GroupBy type, const QString &key) const {

  // Pretty display text for the dividers.

  switch (type) {
    case GroupBy_AlbumArtist:
    case GroupBy_Artist:
    case GroupBy_Album:
    case GroupBy_AlbumDisc:
    case GroupBy_Composer:
    case GroupBy_Performer:
    case GroupBy_Disc:
    case GroupBy_Grouping:
    case GroupBy_Genre:
    case GroupBy_FileType:
    case GroupBy_Format:
    if (key == "0") return "0-9";
      return key.toUpper();

    case GroupBy_YearAlbum:
    case GroupBy_YearAlbumDisc:
    case GroupBy_OriginalYearAlbum:
    case GroupBy_OriginalYearAlbumDisc:
      if (key == "0000") return tr("Unknown");
      return key.toUpper();

    case GroupBy_Year:
    case GroupBy_OriginalYear:
      if (key == "0000") return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy_Samplerate:
      if (key == "000") return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy_Bitdepth:
      if (key == "000") return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy_Bitrate:
      if (key == "000") return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy_None:
    case GroupByCount:
      // fallthrough
      ;
  }
  qLog(Error) << "Unknown GroupBy type" << type << "for divider key" << key;
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
      if (node->container_level == 0)
        divider_keys << DividerKey(group_by_[0], node);

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
      QMap<quint64, ItemAndCacheKey>::iterator i = pending_art_.begin();
      while (i != pending_art_.end()) {
        if (i.value().first == node) {
          i = pending_art_.erase(i);
        }
        else {
          ++i;
        }
      }

      // It was empty - delete it
      beginRemoveRows(ItemToIndex(node->parent), node->row, node->row);
      node->parent->Delete(node->row);
      endRemoveRows();
    }
  }

  // Delete empty dividers
  for (const QString &divider_key : divider_keys) {
    if (!divider_nodes_.contains(divider_key)) continue;

    // Look to see if there are any other items still under this divider
    bool found = false;
    for (CollectionItem *node : container_nodes_[0].values()) {
      if (DividerKey(group_by_[0], node) == divider_key) {
        found = true;
        break;
      }
    }

    if (found) continue;

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

  return Song::TextForSource(backend_->Source()) + "/" + path.join("/");

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
  if (result.image_scaled.isNull()) {
    // Set the no_cover image so we don't continually try to load art.
    QPixmapCache::insert(cache_key, no_cover_icon_);
  }
  else {
    QPixmap image_pixmap;
    image_pixmap = QPixmap::fromImage(result.image_scaled);
    QPixmapCache::insert(cache_key, image_pixmap);
  }

  // If we have a valid cover not already in the disk cache
  if (use_disk_cache_ && sIconCache) {
    std::unique_ptr<QIODevice> cached_img(sIconCache->data(QUrl(cache_key)));
    if (!cached_img && !result.image_scaled.isNull()) {
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
      GroupBy container_type = group_by_[item->container_level];
      is_album_node = IsAlbumGroupBy(container_type);
    }
    if (is_album_node) {
      // It has const behaviour some of the time - that's ok right?
      return const_cast<CollectionModel*>(this)->AlbumIcon(idx);
    }
  }

  return data(item, role);

}

QVariant CollectionModel::data(const CollectionItem *item, const int role) const {

  GroupBy container_type = item->type == CollectionItem::Type_Container ? group_by_[item->container_level] : GroupBy_None;

  switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
      return item->DisplayText();

    case Qt::DecorationRole:
      switch (item->type) {
        case CollectionItem::Type_Container:
          switch (container_type) {
            case GroupBy_Album:
            case GroupBy_AlbumDisc:
            case GroupBy_YearAlbum:
            case GroupBy_YearAlbumDisc:
            case GroupBy_OriginalYearAlbum:
            case GroupBy_OriginalYearAlbumDisc:
              return album_icon_;
            case GroupBy_Artist:
            case GroupBy_AlbumArtist:
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
      return container_type;

    case Role_Key:
      return item->key;

    case Role_Artist:
      return item->metadata.artist();

    case Role_Editable:
      if (!item->lazy_loaded) {
        const_cast<CollectionModel*>(this)->LazyPopulate(const_cast<CollectionItem*>(item), true);
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

bool CollectionModel::HasCompilations(const CollectionQuery &query) {

  CollectionQuery q = query;
  q.AddCompilationRequirement(true);
  q.SetLimit(1);

  QMutexLocker l(backend_->db()->Mutex());
  if (!backend_->ExecQuery(&q)) return false;

  return q.Next();

}

CollectionModel::QueryResult CollectionModel::RunQuery(CollectionItem *parent) {

  QueryResult result;

  // Information about what we want the children to be
  int child_level = parent == root_ ? 0 : parent->container_level + 1;
  GroupBy child_type = child_level >= 3 ? GroupBy_None : group_by_[child_level];

  // Initialize the query.  child_type says what type of thing we want (artists, songs, etc.)
  CollectionQuery q(query_options_);
  InitQuery(child_type, &q);

  // Walk up through the item's parents adding filters as necessary
  CollectionItem *p = parent;
  while (p && p->type == CollectionItem::Type_Container) {
    FilterQuery(group_by_[p->container_level], p, &q);
    p = p->parent;
  }

  // Artists GroupBy is special - we don't want compilation albums appearing
  if (IsArtistGroupBy(child_type)) {
    // Add the special Various artists node
    if (show_various_artists_ && HasCompilations(q)) {
      result.create_va = true;
    }

    // Don't show compilations again outside the Various artists node
    q.AddCompilationRequirement(false);
  }

  // Execute the query
  QMutexLocker l(backend_->db()->Mutex());
  if (backend_->ExecQuery(&q)) {
    while (q.Next()) {
      result.rows << SqlRow(q);
    }
  }

  if (QThread::currentThread() != thread() && QThread::currentThread() != backend_->thread()) {
    backend_->Close();
  }

  return result;

}

void CollectionModel::PostQuery(CollectionItem *parent, const CollectionModel::QueryResult &result, const bool signal) {

  // Information about what we want the children to be
  int child_level = parent == root_ ? 0 : parent->container_level + 1;
  GroupBy child_type = child_level >= 3 ? GroupBy_None : group_by_[child_level];

  if (result.create_va && parent->compilation_artist_node_ == nullptr) {
    CreateCompilationArtistNode(signal, parent);
  }

  // Step through the results
  for (const SqlRow &row : result.rows) {
    // Create the item - it will get inserted into the model here
    CollectionItem *item = ItemFromQuery(child_type, signal, child_level == 0, parent, row, child_level);

    // Save a pointer to it for later
    if (child_type == GroupBy_None) {
      song_nodes_[item->metadata.id()] = item;
    }
    else {
      container_nodes_[child_level][item->key] = item;
    }
  }

}

void CollectionModel::LazyPopulate(CollectionItem *parent, const bool signal) {

  if (parent->lazy_loaded) return;
  parent->lazy_loaded = true;

  QueryResult result = RunQuery(parent);
  PostQuery(parent, result, signal);

}

void CollectionModel::ResetAsync() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QFuture<CollectionModel::QueryResult> future = QtConcurrent::run(&CollectionModel::RunQuery, this, root_);
#else
  QFuture<CollectionModel::QueryResult> future = QtConcurrent::run(this, &CollectionModel::RunQuery, root_);
#endif
  NewClosure(future, this, SLOT(ResetAsyncQueryFinished(QFuture<CollectionModel::QueryResult>)), future);
}

void CollectionModel::ResetAsyncQueryFinished(QFuture<CollectionModel::QueryResult> future) {

  if (QThread::currentThread() != thread() && QThread::currentThread() != backend_->thread()) {
    backend_->Close();
  }

  const struct QueryResult result = future.result();

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

void CollectionModel::InitQuery(const GroupBy type, CollectionQuery *q) {

  // Say what type of thing we want to get back from the database.
  switch (type) {
    case GroupBy_AlbumArtist:
      q->SetColumnSpec("DISTINCT effective_albumartist");
      break;
    case GroupBy_Artist:
      q->SetColumnSpec("DISTINCT artist");
      break;
    case GroupBy_Album:
      q->SetColumnSpec("DISTINCT album, album_id");
      break;
    case GroupBy_AlbumDisc:
      q->SetColumnSpec("DISTINCT album, album_id, disc");
      break;
    case GroupBy_YearAlbum:
      q->SetColumnSpec("DISTINCT year, album, album_id, grouping");
      break;
    case GroupBy_YearAlbumDisc:
      q->SetColumnSpec("DISTINCT year, album, album_id, disc");
      break;
    case GroupBy_OriginalYearAlbum:
      q->SetColumnSpec("DISTINCT year, originalyear, album, album_id, grouping");
      break;
    case GroupBy_OriginalYearAlbumDisc:
      q->SetColumnSpec("DISTINCT year, originalyear, album, album_id, disc");
      break;
    case GroupBy_Disc:
      q->SetColumnSpec("DISTINCT disc");
      break;
    case GroupBy_Year:
      q->SetColumnSpec("DISTINCT year");
      break;
    case GroupBy_OriginalYear:
      q->SetColumnSpec("DISTINCT effective_originalyear");
      break;
    case GroupBy_Genre:
      q->SetColumnSpec("DISTINCT genre");
      break;
    case GroupBy_Composer:
      q->SetColumnSpec("DISTINCT composer");
      break;
    case GroupBy_Performer:
      q->SetColumnSpec("DISTINCT performer");
      break;
    case GroupBy_Grouping:
      q->SetColumnSpec("DISTINCT grouping");
      break;
    case GroupBy_FileType:
      q->SetColumnSpec("DISTINCT filetype");
      break;
    case GroupBy_Format:
      q->SetColumnSpec("DISTINCT filetype, samplerate, bitdepth");
      break;
    case GroupBy_Samplerate:
      q->SetColumnSpec("DISTINCT samplerate");
      break;
    case GroupBy_Bitdepth:
      q->SetColumnSpec("DISTINCT bitdepth");
      break;
    case GroupBy_Bitrate:
      q->SetColumnSpec("DISTINCT bitrate");
      break;
    case GroupBy_None:
    case GroupByCount:
      q->SetColumnSpec("%songs_table.ROWID, " + Song::kColumnSpec);
      break;
  }

}

void CollectionModel::FilterQuery(const GroupBy type, CollectionItem *item, CollectionQuery *q) {

  // Say how we want the query to be filtered.  This is done once for each parent going up the tree.

  switch (type) {
    case GroupBy_AlbumArtist:
      if (IsCompilationArtistNode(item)) {
        q->AddCompilationRequirement(true);
      }
      else {
        // Don't duplicate compilations outside the Various artists node
        q->AddCompilationRequirement(false);
        q->AddWhere("effective_albumartist", item->metadata.effective_albumartist());
      }
      break;
    case GroupBy_Artist:
      if (IsCompilationArtistNode(item)) {
        q->AddCompilationRequirement(true);
      }
      else {
        // Don't duplicate compilations outside the Various artists node
        q->AddCompilationRequirement(false);
        q->AddWhere("artist", item->metadata.artist());
      }
      break;
    case GroupBy_Album:
      q->AddWhere("album", item->metadata.album());
      q->AddWhere("album_id", item->metadata.album_id());
      break;
    case GroupBy_AlbumDisc:
      q->AddWhere("album", item->metadata.album());
      q->AddWhere("album_id", item->metadata.album_id());
      q->AddWhere("disc", item->metadata.disc());
      break;
    case GroupBy_YearAlbum:
      q->AddWhere("year", item->metadata.year());
      q->AddWhere("album", item->metadata.album());
      q->AddWhere("album_id", item->metadata.album_id());
      q->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy_YearAlbumDisc:
      q->AddWhere("year", item->metadata.year());
      q->AddWhere("album", item->metadata.album());
      q->AddWhere("album_id", item->metadata.album_id());
      q->AddWhere("disc", item->metadata.disc());
      break;
    case GroupBy_OriginalYearAlbum:
      q->AddWhere("year", item->metadata.year());
      q->AddWhere("originalyear", item->metadata.originalyear());
      q->AddWhere("album", item->metadata.album());
      q->AddWhere("album_id", item->metadata.album_id());
      q->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy_OriginalYearAlbumDisc:
      q->AddWhere("year", item->metadata.year());
      q->AddWhere("originalyear", item->metadata.originalyear());
      q->AddWhere("album", item->metadata.album());
      q->AddWhere("album_id", item->metadata.album_id());
      q->AddWhere("disc", item->metadata.disc());
      break;
    case GroupBy_Disc:
      q->AddWhere("disc", item->metadata.disc());
      break;
    case GroupBy_Year:
      q->AddWhere("year", item->metadata.year());
      break;
    case GroupBy_OriginalYear:
      q->AddWhere("effective_originalyear", item->metadata.effective_originalyear());
      break;
    case GroupBy_Genre:
      q->AddWhere("genre", item->metadata.genre());
      break;
    case GroupBy_Composer:
      q->AddWhere("composer", item->metadata.composer());
      break;
    case GroupBy_Performer:
      q->AddWhere("performer", item->metadata.performer());
      break;
    case GroupBy_Grouping:
      q->AddWhere("grouping", item->metadata.grouping());
      break;
    case GroupBy_FileType:
      q->AddWhere("filetype", item->metadata.filetype());
      break;
    case GroupBy_Format:
      q->AddWhere("filetype", item->metadata.filetype());
      q->AddWhere("samplerate", item->metadata.samplerate());
      q->AddWhere("bitdepth", item->metadata.bitdepth());
      break;
    case GroupBy_Samplerate:
      q->AddWhere("samplerate", item->metadata.samplerate());
      break;
    case GroupBy_Bitdepth:
      q->AddWhere("bitdepth", item->metadata.bitdepth());
      break;
    case GroupBy_Bitrate:
      q->AddWhere("bitrate", item->metadata.bitrate());
      break;
    case GroupBy_None:
    case GroupByCount:
      qLog(Error) << "Unknown GroupBy type" << type << "used in filter";
      break;
  }

}

CollectionItem *CollectionModel::InitItem(const GroupBy type, const bool signal, CollectionItem *parent, const int container_level) {

  CollectionItem::Type item_type = type == GroupBy_None ? CollectionItem::Type_Song : CollectionItem::Type_Container;

  if (signal) beginInsertRows(ItemToIndex(parent), parent->children.count(), parent->children.count());

  // Initialize the item depending on what type it's meant to be
  CollectionItem *item = new CollectionItem(item_type, parent);
  item->compilation_artist_node_ = nullptr;
  item->container_level = container_level;

  return item;

}

CollectionItem *CollectionModel::ItemFromQuery(const GroupBy type, const bool signal, const bool create_divider, CollectionItem *parent, const SqlRow &row, const int container_level) {

  CollectionItem *item = InitItem(type, signal, parent, container_level);

  if (parent != root_ && !parent->key.isEmpty()) {
    item->key = parent->key + "-";
  }

  switch (type) {
    case GroupBy_AlbumArtist:{
      item->metadata.set_albumartist(row.value(0).toString());
      item->key.append(TextOrUnknown(item->metadata.albumartist()));
      item->display_text = TextOrUnknown(item->metadata.albumartist());
      item->sort_text = SortTextForArtist(item->metadata.albumartist());
      break;
    }
    case GroupBy_Artist:{
      item->metadata.set_artist(row.value(0).toString());
      item->key.append(TextOrUnknown(item->metadata.artist()));
      item->display_text = TextOrUnknown(item->metadata.artist());
      item->sort_text = SortTextForArtist(item->metadata.artist());
      break;
    }
    case GroupBy_Album:{
      item->metadata.set_album(row.value(0).toString());
      item->metadata.set_album_id(row.value(1).toString());
      item->key.append(TextOrUnknown(item->metadata.album()));
      item->display_text = TextOrUnknown(item->metadata.album());
      item->sort_text = SortTextForArtist(item->metadata.album());
      break;
    }
    case GroupBy_AlbumDisc:{
      item->metadata.set_album(row.value(0).toString());
      item->metadata.set_album_id(row.value(1).toString());
      item->metadata.set_disc(row.value(2).toInt());
      item->key.append(PrettyAlbumDisc(item->metadata.album(), item->metadata.disc()));
      const int disc = qMax(0, item->metadata.disc());
      item->display_text = PrettyAlbumDisc(item->metadata.album(), item->metadata.disc());
      item->sort_text = item->metadata.album() + SortTextForNumber(disc);
      break;
    }
    case GroupBy_YearAlbum:{
      item->metadata.set_year(row.value(0).toInt());
      item->metadata.set_album(row.value(1).toString());
      item->metadata.set_album_id(row.value(2).toString());
      item->metadata.set_grouping(row.value(3).toString());
      item->key.append(PrettyYearAlbum(item->metadata.year(), item->metadata.album()));
      item->display_text = PrettyYearAlbum(item->metadata.year(), item->metadata.album());
      item->sort_text = SortTextForNumber(qMax(0, item->metadata.year())) + item->metadata.grouping() + item->metadata.album();
      break;
    }
    case GroupBy_YearAlbumDisc:{
      item->metadata.set_year(row.value(0).toInt());
      item->metadata.set_album(row.value(1).toString());
      item->metadata.set_album_id(row.value(2).toString());
      item->metadata.set_disc(row.value(3).toInt());
      item->key.append(PrettyYearAlbumDisc(item->metadata.year(), item->metadata.album(), item->metadata.disc()));
      item->display_text = PrettyYearAlbumDisc(item->metadata.year(), item->metadata.album(), item->metadata.disc());
      item->sort_text = SortTextForNumber(qMax(0, item->metadata.year())) + item->metadata.album() + SortTextForNumber(qMax(0, item->metadata.disc()));
      break;
    }
    case GroupBy_OriginalYearAlbum:{
      item->metadata.set_year(row.value(0).toInt());
      item->metadata.set_originalyear(row.value(1).toInt());
      item->metadata.set_album(row.value(2).toString());
      item->metadata.set_album_id(row.value(3).toString());
      item->metadata.set_grouping(row.value(4).toString());
      item->key.append(PrettyYearAlbum(item->metadata.effective_originalyear(), item->metadata.album()));
      item->display_text = PrettyYearAlbum(item->metadata.effective_originalyear(), item->metadata.album());
      item->sort_text = SortTextForNumber(qMax(0, item->metadata.effective_originalyear())) + item->metadata.grouping() + item->metadata.album();
      break;
    }
    case GroupBy_OriginalYearAlbumDisc:{
      item->metadata.set_year(row.value(0).toInt());
      item->metadata.set_originalyear(row.value(1).toInt());
      item->metadata.set_album(row.value(2).toString());
      item->metadata.set_album_id(row.value(3).toString());
      item->metadata.set_disc(row.value(4).toInt());
      item->key.append(PrettyYearAlbumDisc(item->metadata.effective_originalyear(), item->metadata.album(), item->metadata.disc()));
      item->display_text = PrettyYearAlbumDisc(item->metadata.effective_originalyear(), item->metadata.album(), item->metadata.disc());
      item->sort_text = SortTextForNumber(qMax(0, item->metadata.effective_originalyear())) + item->metadata.album() + SortTextForNumber(qMax(0, item->metadata.disc()));
      break;
    }
    case GroupBy_Disc:{
      item->metadata.set_disc(row.value(0).toInt());
      const int disc = qMax(0, row.value(0).toInt());
      item->key.append(PrettyDisc(disc));
      item->display_text = PrettyDisc(disc);
      item->sort_text = SortTextForNumber(disc);
      break;
    }
    case GroupBy_Year:{
      item->metadata.set_year(row.value(0).toInt());
      const int year = qMax(0, item->metadata.year());
      item->key.append(QString::number(year));
      item->display_text = QString::number(year);
      item->sort_text = SortTextForNumber(year) + " ";
      break;
    }
    case GroupBy_OriginalYear:{
      item->metadata.set_originalyear(row.value(0).toInt());
      const int year = qMax(0, item->metadata.originalyear());
      item->key.append(QString::number(year));
      item->display_text = QString::number(year);
      item->sort_text = SortTextForNumber(year) + " ";
      break;
    }
    case GroupBy_Genre:{
      item->metadata.set_genre(row.value(0).toString());
      item->key.append(TextOrUnknown(item->metadata.genre()));
      item->display_text = TextOrUnknown(item->metadata.genre());
      item->sort_text = SortTextForArtist(item->metadata.genre());
      break;
    }
    case GroupBy_Composer:{
      item->metadata.set_composer(row.value(0).toString());
      item->key.append(TextOrUnknown(item->metadata.composer()));
      item->display_text = TextOrUnknown(item->metadata.composer());
      item->sort_text = SortTextForArtist(item->metadata.composer());
      break;
    }
    case GroupBy_Performer:{
      item->metadata.set_performer(row.value(0).toString());
      item->key.append(TextOrUnknown(item->metadata.performer()));
      item->display_text = TextOrUnknown(item->metadata.performer());
      item->sort_text = SortTextForArtist(item->metadata.performer());
      break;
    }
    case GroupBy_Grouping:{
      item->metadata.set_grouping(row.value(0).toString());
      item->key.append(TextOrUnknown(item->metadata.grouping()));
      item->display_text = TextOrUnknown(item->metadata.grouping());
      item->sort_text = SortTextForArtist(item->metadata.grouping());
      break;
    }
    case GroupBy_FileType:{
      item->metadata.set_filetype(Song::FileType(row.value(0).toInt()));
      item->key.append(item->metadata.TextForFiletype());
      item->display_text = item->metadata.TextForFiletype();
      item->sort_text = item->metadata.TextForFiletype();
      break;
    }
    case GroupBy_Format:{
      item->metadata.set_filetype(Song::FileType(row.value(0).toInt()));
      item->metadata.set_samplerate(row.value(1).toInt());
      item->metadata.set_bitdepth(row.value(2).toInt());
      QString key;
      if (item->metadata.samplerate() <= 0) {
        key = item->metadata.TextForFiletype();
      }
      else {
        if (item->metadata.bitdepth() <= 0) {
          key = QString("%1 (%2)").arg(item->metadata.TextForFiletype()).arg(QString::number(item->metadata.samplerate() / 1000.0, 'G', 5));
        }
        else {
          key = QString("%1 (%2/%3)").arg(item->metadata.TextForFiletype()).arg(QString::number(item->metadata.samplerate() / 1000.0, 'G', 5)).arg(QString::number(item->metadata.bitdepth()));
        }
      }
      item->display_text = key;
      item->sort_text = key;
      item->key.append(key);
      break;
    }
    case GroupBy_Samplerate:{
      item->metadata.set_samplerate(row.value(0).toInt());
      const int samplerate = qMax(0, item->metadata.samplerate());
      item->key.append(QString::number(samplerate));
      item->display_text = QString::number(samplerate);
      item->sort_text = SortTextForNumber(samplerate) + " ";
      break;
    }
    case GroupBy_Bitdepth:{
      item->metadata.set_bitdepth(row.value(0).toInt());
      const int bitdepth = qMax(0, item->metadata.bitdepth());
      item->key.append(QString::number(bitdepth));
      item->display_text = QString::number(bitdepth);
      item->sort_text = SortTextForNumber(bitdepth) + " ";
      break;
    }
    case GroupBy_Bitrate:{
      item->metadata.set_bitrate(row.value(0).toInt());
      const int bitrate = qMax(0, item->metadata.bitrate());
      item->key.append(QString::number(bitrate));
      item->display_text = QString::number(bitrate);
      item->sort_text = SortTextForNumber(bitrate) + " ";
      break;
    }
    case GroupBy_None:
    case GroupByCount:
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

  FinishItem(type, signal, create_divider, parent, item);

  return item;

}

CollectionItem *CollectionModel::ItemFromSong(const GroupBy type, const bool signal, const bool create_divider, CollectionItem *parent, const Song &s, const int container_level) {

  CollectionItem *item = InitItem(type, signal, parent, container_level);

  if (parent != root_ && !parent->key.isEmpty()) {
    item->key = parent->key + "-";
  }

  switch (type) {
    case GroupBy_AlbumArtist:{
      item->metadata.set_albumartist(s.effective_albumartist());
      item->key.append(TextOrUnknown(s.effective_albumartist()));
      item->display_text = TextOrUnknown(s.effective_albumartist());
      item->sort_text = SortTextForArtist(s.effective_albumartist());
      break;
    }
    case GroupBy_Artist:{
      item->metadata.set_artist(s.artist());
      item->key.append(TextOrUnknown(s.artist()));
      item->display_text = TextOrUnknown(s.artist());
      item->sort_text = SortTextForArtist(s.artist());
      break;
    }
    case GroupBy_Album:{
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->key.append(TextOrUnknown(s.album()));
      item->display_text = TextOrUnknown(s.album());
      item->sort_text = SortTextForArtist(s.album());
      break;
    }
    case GroupBy_AlbumDisc:{
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_disc(s.disc());
      item->key.append(PrettyAlbumDisc(s.album(), s.disc()));
      item->display_text = PrettyAlbumDisc(s.album(), s.disc());
      item->sort_text = s.album() + SortTextForNumber(qMax(0, s.disc()));
      break;
    }
    case GroupBy_YearAlbum:{
      item->metadata.set_year(s.year());
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_grouping(s.grouping());
      item->key.append(PrettyYearAlbum(s.year(), s.album()));
      item->display_text = PrettyYearAlbum(s.year(), s.album());
      item->sort_text = SortTextForNumber(qMax(0, s.year())) + s.grouping() + s.album();
      break;
    }
    case GroupBy_YearAlbumDisc:{
      item->metadata.set_year(s.year());
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_disc(s.disc());
      item->key.append(PrettyYearAlbumDisc(s.year(), s.album(), s.disc()));
      item->display_text = PrettyYearAlbumDisc(s.year(), s.album(), s.disc());
      item->sort_text = SortTextForNumber(qMax(0, s.year())) + s.album() + SortTextForNumber(qMax(0, s.disc()));
      break;
    }
    case GroupBy_OriginalYearAlbum:{
      item->metadata.set_year(s.year());
      item->metadata.set_originalyear(s.originalyear());
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_grouping(s.grouping());
      item->key.append(PrettyYearAlbum(s.effective_originalyear(), s.album()));
      item->display_text = PrettyYearAlbum(s.effective_originalyear(), s.album());
      item->sort_text = SortTextForNumber(qMax(0, s.effective_originalyear())) + s.grouping() + s.album();
      break;
    }
    case GroupBy_OriginalYearAlbumDisc:{
      item->metadata.set_year(s.year());
      item->metadata.set_originalyear(s.originalyear());
      item->metadata.set_album(s.album());
      item->metadata.set_album_id(s.album_id());
      item->metadata.set_disc(s.disc());
      item->metadata.set_grouping(s.grouping());
      item->key.append(PrettyYearAlbumDisc(s.effective_originalyear(), s.album(), s.disc()));
      item->display_text = PrettyYearAlbumDisc(s.effective_originalyear(), s.album(), s.disc());
      item->sort_text = SortTextForNumber(qMax(0, s.effective_originalyear())) + s.album() + SortTextForNumber(qMax(0, s.disc()));
      break;
    }
    case GroupBy_Year:{
      item->metadata.set_year(s.year());
      const int year = qMax(0, s.year());
      item->key.append(QString::number(year));
      item->display_text = QString::number(year);
      item->sort_text = SortTextForNumber(year) + " ";
      break;
    }
    case GroupBy_OriginalYear:{
      item->metadata.set_originalyear(s.effective_originalyear());
      const int year = qMax(0, s.effective_originalyear());
      item->key.append(QString::number(year));
      item->display_text = QString::number(year);
      item->sort_text = SortTextForNumber(year) + " ";
      break;
    }
    case GroupBy_Composer:{
      item->metadata.set_composer(s.composer());
      item->key.append(TextOrUnknown(s.composer()));
      item->display_text = TextOrUnknown(s.composer());
      item->sort_text = SortTextForArtist(s.composer());
      break;
    }
    case GroupBy_Performer:{
      item->metadata.set_performer(s.performer());
      item->key.append(TextOrUnknown(s.performer()));
      item->display_text = TextOrUnknown(s.performer());
      item->sort_text = SortTextForArtist(s.performer());
      break;
    }
    case GroupBy_Grouping:{
      item->metadata.set_grouping(s.grouping());
      item->key.append(TextOrUnknown(s.grouping()));
      item->display_text = TextOrUnknown(s.grouping());
      item->sort_text = SortTextForArtist(s.grouping());
      break;
    }
    case GroupBy_Genre:{
      item->metadata.set_genre(s.genre());
      item->key.append(TextOrUnknown(s.genre()));
      item->display_text = TextOrUnknown(s.genre());
      item->sort_text = SortTextForArtist(s.genre());
      break;
    }
    case GroupBy_Disc:{
      item->metadata.set_disc(s.disc());
      const int disc = qMax(0, s.disc());
      item->key.append(PrettyDisc(disc));
      item->display_text = PrettyDisc(disc);
      item->sort_text = SortTextForNumber(disc);
      break;
    }
    case GroupBy_FileType:
      item->metadata.set_filetype(s.filetype());
      item->key.append(s.TextForFiletype());
      item->display_text = s.TextForFiletype();
      item->sort_text = s.TextForFiletype();
      break;
    case GroupBy_Bitrate:{
      item->metadata.set_bitrate(s.bitrate());
      const int bitrate = qMax(0, s.bitrate());
      item->key.append(QString::number(bitrate));
      item->display_text = QString::number(bitrate);
      item->sort_text = SortTextForNumber(bitrate) + " ";
      break;
    }
    case GroupBy_Samplerate:{
      item->metadata.set_samplerate(s.samplerate());
      const int samplerate = qMax(0, s.samplerate());
      item->key.append(QString::number(samplerate));
      item->display_text = QString::number(samplerate);
      item->sort_text = SortTextForNumber(samplerate) + " ";
      break;
    }
    case GroupBy_Bitdepth:{
      item->metadata.set_bitdepth(s.bitdepth());
      const int bitdepth = qMax(0, s.bitdepth());
      item->key.append(QString::number(bitdepth));
      item->display_text = QString::number(bitdepth);
      item->sort_text = SortTextForNumber(bitdepth) + " ";
      break;
    }
    case GroupBy_Format:{
      item->metadata.set_filetype(s.filetype());
      item->metadata.set_samplerate(s.samplerate());
      item->metadata.set_bitdepth(s.bitdepth());
      QString key;
      if (s.samplerate() <= 0) {
        key = s.TextForFiletype();
      }
      else {
        if (s.bitdepth() <= 0) {
          key = QString("%1 (%2)").arg(s.TextForFiletype()).arg(QString::number(s.samplerate() / 1000.0, 'G', 5));
        }
        else {
          key = QString("%1 (%2/%3)").arg(s.TextForFiletype()).arg(QString::number(s.samplerate() / 1000.0, 'G', 5)).arg(QString::number(s.bitdepth()));
        }
      }
      item->key.append(key);
      item->display_text = key;
      item->sort_text = key;
      break;
    }
    case GroupBy_None:
    case GroupByCount:{
      item->metadata = s;
      item->key.append(s.title());
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

  FinishItem(type, signal, create_divider, parent, item);
  if (s.url().scheme() == "cdda") item->lazy_loaded = true;

  return item;

}

void CollectionModel::FinishItem(const GroupBy type, const bool signal, const bool create_divider, CollectionItem *parent, CollectionItem *item) {

  if (type == GroupBy_None) item->lazy_loaded = true;

  if (signal) endInsertRows();

  // Create the divider entry if we're supposed to
  if (create_divider && show_dividers_) {
    QString divider_key = DividerKey(type, item);
    if (!divider_key.isEmpty()) {
      item->sort_text.prepend(divider_key + " ");
    }

    if (!divider_key.isEmpty() && !divider_nodes_.contains(divider_key)) {
      if (signal)
        beginInsertRows(ItemToIndex(parent), parent->children.count(), parent->children.count());

      CollectionItem *divider = new CollectionItem(CollectionItem::Type_Divider, root_);
      divider->key = divider_key;
      divider->display_text = DividerDisplayText(type, divider_key);
      divider->sort_text = divider_key + "  ";
      divider->lazy_loaded = true;

      divider_nodes_[divider_key] = divider;

      if (signal) endInsertRows();
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

  if (disc <= 0 || album.contains(QRegularExpression(Song::kAlbumRemoveDisc))) return TextOrUnknown(album);
  else return TextOrUnknown(album) + " - (Disc " + QString::number(disc) + ")";

}

QString CollectionModel::PrettyYearAlbumDisc(const int year, const QString &album, const int disc) {

  QString str;

  if (year <= 0) str = TextOrUnknown(album);
  else str = QString::number(year) + " - " + TextOrUnknown(album);

  if (!album.contains(QRegularExpression(Song::kAlbumRemoveDisc)) && disc > 0) str += " - (Disc " + QString::number(disc) + ")";

  return str;

}

QString CollectionModel::PrettyDisc(const int disc) {

  return "Disc " + QString::number(qMax(1, disc));

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
      int ilen = i.length();
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

  QString ret = QString::number(qMax(0, song.disc()) * 1000 + qMax(0, song.track()));
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

int CollectionModel::MaximumCacheSize(QSettings *s, const char *size_id, const char *size_unit_id, const int cache_size_default) const {

  int size = s->value(size_id, cache_size_default).toInt();
  int unit = s->value(size_unit_id, CollectionSettingsPage::CacheSizeUnit::CacheSizeUnit_MB).toInt() + 1;

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
      std::sort(children.begin(), children.end(), std::bind(&CollectionModel::CompareItems, this, _1, _2));

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

void CollectionModel::SetFilterAge(const int age) {
  query_options_.set_max_age(age);
  ResetAsync();
}

void CollectionModel::SetFilterText(const QString &text) {
  query_options_.set_filter(text);
  ResetAsync();

}

void CollectionModel::SetFilterQueryMode(QueryOptions::QueryMode query_mode) {
  query_options_.set_query_mode(query_mode);
  ResetAsync();

}

bool CollectionModel::canFetchMore(const QModelIndex &parent) const {

  if (!parent.isValid()) return false;

  CollectionItem *item = IndexToItem(parent);
  return !item->lazy_loaded;

}

void CollectionModel::SetGroupBy(const Grouping &g) {

  group_by_ = g;

  ResetAsync();
  emit GroupingChanged(g);

}

const CollectionModel::GroupBy &CollectionModel::Grouping::operator[](const int i) const {

  switch (i) {
    case 0: return first;
    case 1: return second;
    case 2: return third;
  }
  qLog(Error) << "CollectionModel::Grouping[] index out of range" << i;
  return first;

}

CollectionModel::GroupBy &CollectionModel::Grouping::operator[](const int i) {

  switch (i) {
    case 0: return first;
    case 1: return second;
    case 2: return third;
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

QDataStream &operator<<(QDataStream &s, const CollectionModel::Grouping &g) {
  s << quint32(g.first) << quint32(g.second) << quint32(g.third);
  return s;
}

QDataStream &operator>>(QDataStream &s, CollectionModel::Grouping &g) {

  quint32 buf;
  s >> buf;
  g.first = CollectionModel::GroupBy(buf);
  s >> buf;
  g.second = CollectionModel::GroupBy(buf);
  s >> buf;
  g.third = CollectionModel::GroupBy(buf);
  return s;

}

