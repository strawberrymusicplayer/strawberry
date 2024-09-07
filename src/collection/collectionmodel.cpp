/*
 * Strawberry Music Player
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <chrono>

#include <QObject>
#include <QtGlobal>
#include <QtConcurrentRun>
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
#include <QTimer>

#include "core/scoped_ptr.h"
#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/database.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "core/sqlrow.h"
#include "core/settings.h"
#include "collectionfilteroptions.h"
#include "collectionquery.h"
#include "collectionbackend.h"
#include "collectiondirectorymodel.h"
#include "collectionitem.h"
#include "collectionmodel.h"
#include "collectionmodelupdate.h"
#include "collectionfilter.h"
#include "playlist/playlistmanager.h"
#include "playlist/songmimedata.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/albumcoverloader.h"
#include "settings/collectionsettingspage.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

const int CollectionModel::kPrettyCoverSize = 32;
namespace {
constexpr char kPixmapDiskCacheDir[] = "pixmapcache";
constexpr char kVariousArtists[] = QT_TR_NOOP("Various artists");
}  // namespace

QNetworkDiskCache *CollectionModel::sIconCache = nullptr;

CollectionModel::CollectionModel(SharedPtr<CollectionBackend> backend, Application *app, QObject *parent)
    : SimpleTreeModel<CollectionItem>(new CollectionItem(this), parent),
      backend_(backend),
      app_(app),
      dir_model_(new CollectionDirectoryModel(backend, this)),
      filter_(new CollectionFilter(this)),
      timer_reload_(new QTimer(this)),
      timer_update_(new QTimer(this)),
      icon_artist_(IconLoader::Load(QStringLiteral("folder-sound"))),
      use_disk_cache_(false),
      total_song_count_(0),
      total_artist_count_(0),
      total_album_count_(0),
      loading_(false) {

  setObjectName(backend_->source() == Song::Source::Collection ? QLatin1String(metaObject()->className()) : QStringLiteral("%1%2").arg(Song::DescriptionForSource(backend_->source()), QLatin1String(metaObject()->className())));

  filter_->setSourceModel(this);
  filter_->setSortRole(Role_SortText);
  filter_->sort(0);

  if (app_) {
    QObject::connect(&*app_->album_cover_loader(), &AlbumCoverLoader::AlbumCoverLoaded, this, &CollectionModel::AlbumCoverLoaded);
  }

  QIcon nocover = IconLoader::Load(QStringLiteral("cdcase"));
  if (!nocover.isNull()) {
    QList<QSize> nocover_sizes = nocover.availableSizes();
    pixmap_no_cover_ = nocover.pixmap(nocover_sizes.last()).scaled(kPrettyCoverSize, kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  if (app_ && !sIconCache) {
    sIconCache = new QNetworkDiskCache(this);
    sIconCache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u'/' + QLatin1String(kPixmapDiskCacheDir));
    QObject::connect(app_, &Application::ClearPixmapDiskCache, this, &CollectionModel::ClearDiskCache);
  }

  QObject::connect(&*backend_, &CollectionBackend::SongsAdded, this, &CollectionModel::AddReAddOrUpdate);
  QObject::connect(&*backend_, &CollectionBackend::SongsChanged, this, &CollectionModel::AddReAddOrUpdate);
  QObject::connect(&*backend_, &CollectionBackend::SongsDeleted, this, &CollectionModel::RemoveSongs);
  QObject::connect(&*backend_, &CollectionBackend::DatabaseReset, this, &CollectionModel::ScheduleReset);
  QObject::connect(&*backend_, &CollectionBackend::TotalSongCountUpdated, this, &CollectionModel::TotalSongCountUpdatedSlot);
  QObject::connect(&*backend_, &CollectionBackend::TotalArtistCountUpdated, this, &CollectionModel::TotalArtistCountUpdatedSlot);
  QObject::connect(&*backend_, &CollectionBackend::TotalAlbumCountUpdated, this, &CollectionModel::TotalAlbumCountUpdatedSlot);
  QObject::connect(&*backend_, &CollectionBackend::SongsStatisticsChanged, this, &CollectionModel::AddReAddOrUpdate);
  QObject::connect(&*backend_, &CollectionBackend::SongsRatingChanged, this, &CollectionModel::AddReAddOrUpdate);

  backend_->UpdateTotalSongCountAsync();
  backend_->UpdateTotalArtistCountAsync();
  backend_->UpdateTotalAlbumCountAsync();

  timer_reload_->setSingleShot(true);
  timer_reload_->setInterval(300ms);
  QObject::connect(timer_reload_, &QTimer::timeout, this, &CollectionModel::Reload);

  timer_update_->setSingleShot(false);
  timer_update_->setInterval(20ms);
  QObject::connect(timer_update_, &QTimer::timeout, this, &CollectionModel::ProcessUpdate);

  ReloadSettings();

}

CollectionModel::~CollectionModel() {

  qLog(Debug) << "Collection model" << this << "deleted";

  beginResetModel();
  Clear();
  endResetModel();

}

void CollectionModel::Init() {
  ScheduleReset();
}

void CollectionModel::Reset() {
  ScheduleReset();
}

void CollectionModel::Clear() {

  if (root_) {
    delete root_;
    root_ = nullptr;
  }
  song_nodes_.clear();
  container_nodes_[0].clear();
  container_nodes_[1].clear();
  container_nodes_[2].clear();
  divider_nodes_.clear();
  pending_art_.clear();
  pending_cache_keys_.clear();

}

void CollectionModel::BeginReset() {

  beginResetModel();
  Clear();
  Q_ASSERT(root_ == nullptr);
  root_ = new CollectionItem(this);

}

void CollectionModel::EndReset() {

  endResetModel();

}

void CollectionModel::Reload() {

  loading_ = true;
  if (timer_reload_->isActive()) {
    timer_reload_->stop();
  }
  updates_.clear();

  options_active_ = options_current_;

  BeginReset();
  // Show a loading indicator in the model.
  CollectionItem *loading = new CollectionItem(CollectionItem::Type::LoadingIndicator, root_);
  loading->display_text = tr("Loading...");
  EndReset();

  LoadSongsFromSqlAsync();

}

void CollectionModel::ScheduleReset() {

  if (!timer_reload_->isActive()) {
    timer_reload_->start();
  }

}

void CollectionModel::ReloadSettings() {

  Settings settings;
  settings.beginGroup(CollectionSettingsPage::kSettingsGroup);
  const bool show_pretty_covers = settings.value("pretty_covers", true).toBool();
  const bool show_dividers= settings.value("show_dividers", true).toBool();
  const bool show_various_artists = settings.value("various_artists", true).toBool();
  const bool sort_skips_articles = settings.value("sort_skips_articles", true).toBool();

  use_disk_cache_ = settings.value(CollectionSettingsPage::kSettingsDiskCacheEnable, false).toBool();
  QPixmapCache::setCacheLimit(static_cast<int>(MaximumCacheSize(&settings, CollectionSettingsPage::kSettingsCacheSize, CollectionSettingsPage::kSettingsCacheSizeUnit, CollectionSettingsPage::kSettingsCacheSizeDefault) / 1024));
  if (sIconCache) {
    sIconCache->setMaximumCacheSize(MaximumCacheSize(&settings, CollectionSettingsPage::kSettingsDiskCacheSize, CollectionSettingsPage::kSettingsDiskCacheSizeUnit, CollectionSettingsPage::kSettingsDiskCacheSizeDefault));
  }

  settings.endGroup();

  cover_types_ = AlbumCoverLoaderOptions::LoadTypes();

  if (show_pretty_covers != options_current_.show_pretty_covers ||
      show_dividers != options_current_.show_dividers ||
      show_various_artists != options_current_.show_various_artists ||
      sort_skips_articles != options_current_.sort_skips_articles) {
    options_current_.show_pretty_covers = show_pretty_covers;
    options_current_.show_dividers = show_dividers;
    options_current_.show_various_artists = show_various_artists;
    options_current_.sort_skips_articles = sort_skips_articles;
    ScheduleReset();
  }

  if (!use_disk_cache_) {
    ClearDiskCache();
  }

}

void CollectionModel::SetGroupBy(const Grouping g, const std::optional<bool> separate_albums_by_grouping) {

  options_current_.group_by = g;
  if (separate_albums_by_grouping) {
    options_current_.separate_albums_by_grouping = separate_albums_by_grouping.value();
  }

  ScheduleReset();

  Q_EMIT GroupingChanged(g, options_current_.separate_albums_by_grouping);

}

void CollectionModel::SetFilterMode(const CollectionFilterOptions::FilterMode filter_mode) {

  if (options_current_.filter_options.filter_mode() != filter_mode) {
    options_current_.filter_options.set_filter_mode(filter_mode);
    ScheduleReset();
  }

}

void CollectionModel::SetFilterMaxAge(const int filter_max_age) {

  if (options_current_.filter_options.max_age() != filter_max_age) {
    options_current_.filter_options.set_max_age(filter_max_age);
    ScheduleReset();
  }

}

QVariant CollectionModel::data(const QModelIndex &idx, const int role) const {

  const CollectionItem *item = IndexToItem(idx);

  // Handle a special case for returning album artwork instead of a generic CD icon.
  // this is here instead of in the other data() function to let us use the
  // QModelIndex& version of GetChildSongs, which satisfies const-ness, instead
  // of the CollectionItem *version, which doesn't.
  if (options_active_.show_pretty_covers) {
    bool is_album_node = false;
    if (role == Qt::DecorationRole && item->type == CollectionItem::Type::Container) {
      GroupBy container_group_by = options_active_.group_by[item->container_level];
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

  GroupBy container_group_by = item->type == CollectionItem::Type::Container ? options_active_.group_by[item->container_level] : GroupBy::None;

  switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
      return item->DisplayText();

    case Qt::DecorationRole:
      switch (item->type) {
        case CollectionItem::Type::Container:
          switch (container_group_by) {
            case GroupBy::Album:
            case GroupBy::AlbumDisc:
            case GroupBy::YearAlbum:
            case GroupBy::YearAlbumDisc:
            case GroupBy::OriginalYearAlbum:
            case GroupBy::OriginalYearAlbumDisc:
              return QVariant();
            case GroupBy::Artist:
            case GroupBy::AlbumArtist:
              return icon_artist_;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;

    case Role_Type:
      return QVariant::fromValue(item->type);

    case Role_IsDivider:
      return item->type == CollectionItem::Type::Divider;

    case Role_ContainerType:
      return static_cast<int>(container_group_by);

    case Role_ContainerKey:
      return item->container_key;

    case Role_Artist:
      return item->metadata.artist();

    case Role_Editable:{
      if (item->type == CollectionItem::Type::Container) {
        // If we have even one non editable item as a child, we ourselves are not available for edit
        if (item->children.isEmpty()) {
          return false;
        }
        if (std::any_of(item->children.begin(), item->children.end(), [this, role](CollectionItem *child) { return !data(child, role).toBool(); })) {
          return false;
        }
        return true;
      }
      if (item->type == CollectionItem::Type::Song) {
        return item->metadata.IsEditable();
      }
      return false;
    }

    case Role_SortText:
      return item->SortText();
    default:
      return QVariant();
  }

  return QVariant();

}

Qt::ItemFlags CollectionModel::flags(const QModelIndex &idx) const {

  switch (IndexToItem(idx)->type) {
    case CollectionItem::Type::LoadingIndicator:
    case CollectionItem::Type::Divider:
      return Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
    case CollectionItem::Type::Container:
    case CollectionItem::Type::Song:
      return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    case CollectionItem::Type::Root:
    default:
      return Qt::ItemIsEnabled;
  }

}

QStringList CollectionModel::mimeTypes() const {
  return QStringList() << QStringLiteral("text/uri-list");
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

void CollectionModel::AddReAddOrUpdate(const SongList &songs) {

  ScheduleUpdate(CollectionModelUpdate::Type::AddReAddOrUpdate, songs);

}

void CollectionModel::RemoveSongs(const SongList &songs) {

  ScheduleRemoveSongs(songs);

}

void CollectionModel::ScheduleUpdate(const CollectionModelUpdate::Type type, const SongList &songs) {

  for (qint64 i = 0; i < songs.count(); i += 400LL) {
    const qint64 number = std::min(songs.count() - i, 400LL);
    const SongList songs_to_queue = songs.mid(i, number);
    updates_.enqueue(CollectionModelUpdate(type, songs_to_queue));
  }

  if (!timer_update_->isActive()) {
    timer_update_->start();
  }

}

void CollectionModel::ScheduleAddSongs(const SongList &songs) {

  ScheduleUpdate(CollectionModelUpdate::Type::Add, songs);

}

void CollectionModel::ScheduleUpdateSongs(const SongList &songs) {

  ScheduleUpdate(CollectionModelUpdate::Type::Update, songs);

}

void CollectionModel::ScheduleRemoveSongs(const SongList &songs) {

  ScheduleUpdate(CollectionModelUpdate::Type::Remove, songs);

}

void CollectionModel::ProcessUpdate() {

  if (loading_ || updates_.isEmpty()) {
    timer_update_->stop();
    return;
  }

  const CollectionModelUpdate update = updates_.dequeue();

  if (updates_.isEmpty()) {
    timer_update_->stop();
  }

  switch (update.type) {
    case CollectionModelUpdate::Type::AddReAddOrUpdate:
      AddReAddOrUpdateSongsInternal(update.songs);
      break;
    case CollectionModelUpdate::Type::Add:
      AddSongsInternal(update.songs);
      break;
    case CollectionModelUpdate::Type::Update:
      UpdateSongsInternal(update.songs);
      break;
    case CollectionModelUpdate::Type::Remove:
      RemoveSongsInternal(update.songs);
      break;
  }

}

void CollectionModel::AddReAddOrUpdateSongsInternal(const SongList &songs) {

  if (loading_) return;

  SongList songs_added;
  SongList songs_removed;
  SongList songs_updated;

  for (const Song &new_song : songs) {
    if (!song_nodes_.contains(new_song.id())) {
      songs_added << new_song;
      continue;
    }
    const Song old_song = song_nodes_.value(new_song.id())->metadata;
    bool container_key_changed = false;
    bool has_unique_album_identifier_1 = false;
    bool has_unique_album_identifier_2 = false;
    for (int i = 0; i < 3; ++i) {
      const GroupBy group_by = options_active_.group_by[i];
      if (group_by == GroupBy::None) break;
      if (options_active_.show_various_artists && IsArtistGroupBy(group_by) && (new_song.is_compilation() || old_song.is_compilation())) {
        has_unique_album_identifier_1 = true;
        has_unique_album_identifier_2 = true;
        if (new_song.is_compilation() != old_song.is_compilation()) {
          container_key_changed = true;
        }
      }
      else if (ContainerKey(group_by, new_song, has_unique_album_identifier_1) != ContainerKey(group_by, old_song, has_unique_album_identifier_2)) {
        container_key_changed = true;
      }
    }

    if (container_key_changed) {
      qLog(Debug) << "Container key for" << new_song.id() << new_song.PrettyTitleWithArtist() << "is changed, re-adding song.";
      songs_removed << old_song;
      songs_added << new_song;
    }
    else {
      qLog(Debug) << "Container key for" << new_song.id() << new_song.PrettyTitleWithArtist() << "is uchanged, only updating song metadata.";
      songs_updated << new_song;
    }
  }

  ScheduleRemoveSongs(songs_removed);
  ScheduleUpdateSongs(songs_updated);
  ScheduleAddSongs(songs_added);

}

void CollectionModel::AddSongsInternal(const SongList &songs) {

  if (loading_) return;

  for (const Song &song : songs) {

    // Sanity check to make sure we don't add songs that are outside the user's filter
    if (!options_active_.filter_options.Matches(song)) continue;

    if (song_nodes_.contains(song.id())) continue;

    // Before we can add each song we need to make sure the required container items already exist in the tree.
    // These depend on which "group by" settings the user has on the collection.
    // Eg. if the user grouped by artist and album, we would need to make sure nodes for the song's artist and album were already in the tree.

    CollectionItem *container = root_;
    QString container_key;
    bool has_unique_album_identifier = false;
    for (int i = 0; i < 3; ++i) {
      const GroupBy group_by = options_active_.group_by[i];
      if (group_by == GroupBy::None) break;
      if (options_active_.show_various_artists && IsArtistGroupBy(group_by) && song.is_compilation()) {
        has_unique_album_identifier = true;
        if (container->compilation_artist_node_ == nullptr) {
          CreateCompilationArtistNode(container);
        }
        container = container->compilation_artist_node_;
        container_key = container->container_key;
      }
      else {
        if (!container_key.isEmpty()) container_key.append(u'-');
        container_key.append(ContainerKey(group_by, song, has_unique_album_identifier));
        if (container_nodes_[i].contains(container_key)) {
          container = container_nodes_[i][container_key];
        }
        else {
          container = CreateContainerItem(group_by, i, container_key, song, container);
        }
      }
    }
    CreateSongItem(song, container);
  }

}

void CollectionModel::UpdateSongsInternal(const SongList &songs) {

  if (loading_) return;

  QList<CollectionItem*> album_parents;

  for (const Song &new_song : songs) {
    if (!song_nodes_.contains(new_song.id())) {
      qLog(Error) << "Song does not exist in model" << new_song.id() << new_song.PrettyTitleWithArtist();
      continue;
    }
    CollectionItem *item = song_nodes_.value(new_song.id());
    const Song &old_song = item->metadata;
    const bool song_title_data_changed = IsSongTitleDataChanged(old_song, new_song);
    const bool art_changed = !old_song.IsArtEqual(new_song);
    SetSongItemData(item, new_song);
    if (art_changed) {
      for (CollectionItem *parent = item->parent; parent != root_; parent = parent->parent) {
        if (IsAlbumGroupBy(options_active_.group_by[parent->container_level])) {
          album_parents << parent;
        }
      }
    }
    if (song_title_data_changed) {
      qLog(Debug) << "Song metadata and title for" << new_song.id() << new_song.PrettyTitleWithArtist() << "changed, informing model";
      const QModelIndex idx = ItemToIndex(item);
      if (!idx.isValid()) continue;
      Q_EMIT dataChanged(idx, idx);
    }
    else {
      qLog(Debug) << "Song metadata for" << new_song.id() << new_song.PrettyTitleWithArtist() << "changed";
    }
  }

  for (CollectionItem *item : std::as_const(album_parents)) {
    ClearItemPixmapCache(item);
    const QModelIndex idx = ItemToIndex(item);
    if (idx.isValid()) {
      Q_EMIT dataChanged(idx, idx);
    }
  }

}

void CollectionModel::RemoveSongsInternal(const SongList &songs) {

  if (loading_) return;

  // Delete the actual song nodes first, keeping track of each parent so we might check to see if they're empty later.
  QSet<CollectionItem*> parents;
  for (const Song &song : songs) {

    if (song_nodes_.contains(song.id())) {
      CollectionItem *node = song_nodes_.value(song.id());

      if (node->parent != root_) parents << node->parent;

      beginRemoveRows(ItemToIndex(node->parent), node->row, node->row);
      node->parent->Delete(node->row);
      song_nodes_.remove(song.id());
      endRemoveRows();

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
        divider_keys << DividerKey(options_active_.group_by[0], node->metadata, node->sort_text);
      }

      // Special case the Various Artists node
      if (IsCompilationArtistNode(node)) {
        node->parent->compilation_artist_node_ = nullptr;
      }
      else if (container_nodes_[node->container_level].contains(node->container_key)) {
        container_nodes_[node->container_level].remove(node->container_key);
      }

      ClearItemPixmapCache(node);

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
    if (std::any_of(container_nodes.begin(), container_nodes.end(), [this, divider_key](CollectionItem *node){ return DividerKey(options_active_.group_by[0], node->metadata, node->sort_text) == divider_key; })) {
      continue;
    }

    // Remove the divider
    const int row = divider_nodes_.value(divider_key)->row;
    beginRemoveRows(ItemToIndex(root_), row, row);
    root_->Delete(row);
    endRemoveRows();
    divider_nodes_.remove(divider_key);
  }

}

CollectionItem *CollectionModel::CreateContainerItem(const GroupBy group_by, const int container_level, const QString &container_key, const Song &song, CollectionItem *parent) {

  QString divider_key;
  if (options_active_.show_dividers && container_level == 0) {
    divider_key = DividerKey(group_by, song, SortText(group_by, container_level, song, options_active_.sort_skips_articles));
    if (!divider_key.isEmpty()) {
      if (!divider_nodes_.contains(divider_key)) {
        CreateDividerItem(divider_key, DividerDisplayText(group_by, divider_key), parent);
      }
    }
  }

  beginInsertRows(ItemToIndex(parent), static_cast<int>(parent->children.count()), static_cast<int>(parent->children.count()));

  CollectionItem *item = new CollectionItem(CollectionItem::Type::Container, parent);
  item->container_level = container_level;
  item->container_key = container_key;
  item->display_text = DisplayText(group_by, song);
  item->sort_text = SortText(group_by, container_level, song, options_active_.sort_skips_articles);
  if (!divider_key.isEmpty()) {
    item->sort_text.prepend(divider_key + QLatin1Char(' '));
  }

  container_nodes_[container_level].insert(item->container_key, item);

  endInsertRows();

  return item;

}

void CollectionModel::CreateDividerItem(const QString &divider_key, const QString &display_text, CollectionItem *parent) {

  beginInsertRows(ItemToIndex(parent), static_cast<int>(parent->children.count()), static_cast<int>(parent->children.count()));

  CollectionItem *divider = new CollectionItem(CollectionItem::Type::Divider, root_);
  divider->container_key = divider_key;
  divider->display_text = display_text;
  divider->sort_text = divider_key + "  "_L1;
  divider_nodes_[divider_key] = divider;

  endInsertRows();

}

void CollectionModel::CreateSongItem(const Song &song, CollectionItem *parent) {

  beginInsertRows(ItemToIndex(parent), static_cast<int>(parent->children.count()), static_cast<int>(parent->children.count()));

  CollectionItem *item = new CollectionItem(CollectionItem::Type::Song, parent);
  SetSongItemData(item, song);
  song_nodes_.insert(song.id(), item);

  endInsertRows();

}

void CollectionModel::SetSongItemData(CollectionItem *item, const Song &song) {

  item->display_text = song.TitleWithCompilationArtist();
  if (item->container_level == 1 && !IsAlbumGroupBy(options_active_.group_by[0])) {
    item->sort_text = SortText(song.title());
  }
  else {
    item->sort_text = SortTextForSong(song);
  }

  item->metadata = song;

}

CollectionItem *CollectionModel::CreateCompilationArtistNode(CollectionItem *parent) {

  Q_ASSERT(parent->compilation_artist_node_ == nullptr);

  beginInsertRows(ItemToIndex(parent), static_cast<int>(parent->children.count()), static_cast<int>(parent->children.count()));

  parent->compilation_artist_node_ = new CollectionItem(CollectionItem::Type::Container, parent);
  parent->compilation_artist_node_->compilation_artist_node_ = nullptr;
  if (parent != root_ && !parent->container_key.isEmpty()) parent->compilation_artist_node_->container_key.append(parent->container_key);
  parent->compilation_artist_node_->container_key.append(QLatin1String(kVariousArtists));
  parent->compilation_artist_node_->display_text = QLatin1String(kVariousArtists);
  parent->compilation_artist_node_->sort_text = " various"_L1;
  parent->compilation_artist_node_->container_level = parent->container_level + 1;

  endInsertRows();

  return parent->compilation_artist_node_;

}

void CollectionModel::LoadSongsFromSqlAsync() {

  QFuture<SongList> future = QtConcurrent::run(&CollectionModel::LoadSongsFromSql, this, options_active_.filter_options);
  QFutureWatcher<SongList> *watcher = new QFutureWatcher<SongList>();
  QObject::connect(watcher, &QFutureWatcher<void>::finished, this, &CollectionModel::LoadSongsFromSqlAsyncFinished);
  watcher->setFuture(future);

}

SongList CollectionModel::LoadSongsFromSql(const CollectionFilterOptions &filter_options) {

  SongList songs;

  {
    QMutexLocker l(backend_->db()->Mutex());
    QSqlDatabase db(backend_->db()->Connect());
    CollectionQuery q(db, backend_->songs_table(), filter_options);
    q.SetColumnSpec(QStringLiteral("%songs_table.ROWID, ") + Song::kColumnSpec);
    if (q.Exec()) {
      while (q.Next()) {
        Song song;
        song.InitFromQuery(q, true);
        songs << song;
      }
    }
    else {
      backend_->ReportErrors(q);
    }
  }

  if (QThread::currentThread() != thread() && QThread::currentThread() != backend_->thread()) {
    backend_->db()->Close();
  }

  return songs;

}

void CollectionModel::LoadSongsFromSqlAsyncFinished() {

  QFutureWatcher<SongList> *watcher = static_cast<QFutureWatcher<SongList>*>(sender());
  const SongList songs = watcher->result();
  watcher->deleteLater();

  BeginReset();
  ScheduleAddSongs(songs);
  EndReset();

  loading_ = false;

  if (!updates_.isEmpty() && !timer_update_->isActive()) {
    timer_update_->start();
  }

}

QString CollectionModel::AlbumIconPixmapCacheKey(const QModelIndex &idx) const {

  return Song::TextForSource(backend_->source()) + QLatin1Char('/') + idx.data(Role_ContainerKey).toString();

}

QUrl CollectionModel::AlbumIconPixmapDiskCacheKey(const QString &cache_key) {

  return QUrl(QString::fromLatin1(QUrl::toPercentEncoding(cache_key)));

}

void CollectionModel::ClearItemPixmapCache(CollectionItem *item) {

  // Remove from pixmap cache
  const QString cache_key = AlbumIconPixmapCacheKey(ItemToIndex(item));
  QPixmapCache::remove(cache_key);
  if (use_disk_cache_ && sIconCache) sIconCache->remove(AlbumIconPixmapDiskCacheKey(cache_key));
  if (pending_cache_keys_.contains(cache_key)) {
    pending_cache_keys_.remove(cache_key);
  }

  // Remove from pending art loading
  for (QMap<quint64, ItemAndCacheKey>::iterator it = pending_art_.begin(); it != pending_art_.end();) {
    if (it.value().first == item) {
      it = pending_art_.erase(it);
    }
    else {
      ++it;
    }
  }

}

QVariant CollectionModel::AlbumIcon(const QModelIndex &idx) {

  CollectionItem *item = IndexToItem(idx);
  if (!item) return pixmap_no_cover_;

  // Check the cache for a pixmap we already loaded.
  const QString cache_key = AlbumIconPixmapCacheKey(idx);

  QPixmap cached_pixmap;
  if (QPixmapCache::find(cache_key, &cached_pixmap)) {
    return cached_pixmap;
  }

  // Try to load it from the disk cache
  if (use_disk_cache_ && sIconCache) {
    ScopedPtr<QIODevice> disk_cache_img(sIconCache->data(AlbumIconPixmapDiskCacheKey(cache_key)));
    if (disk_cache_img) {
      QImage cached_image;
      if (cached_image.load(&*disk_cache_img, "XPM")) {
        QPixmapCache::insert(cache_key, QPixmap::fromImage(cached_image));
        return QPixmap::fromImage(cached_image);
      }
    }
  }

  // Maybe we're loading a pixmap already?
  if (pending_cache_keys_.contains(cache_key)) {
    return pixmap_no_cover_;
  }

  // No art is cached and we're not loading it already.  Load art for the first song in the album.
  SongList songs = GetChildSongs(idx);
  if (!songs.isEmpty()) {
    AlbumCoverLoaderOptions cover_loader_options(AlbumCoverLoaderOptions::Option::ScaledImage | AlbumCoverLoaderOptions::Option::PadScaledImage);
    cover_loader_options.desired_scaled_size = QSize(kPrettyCoverSize, kPrettyCoverSize);
    cover_loader_options.types = cover_types_;
    const quint64 id = app_->album_cover_loader()->LoadImageAsync(cover_loader_options, songs.first());
    pending_art_[id] = ItemAndCacheKey(item, cache_key);
    pending_cache_keys_.insert(cache_key);
  }

  return pixmap_no_cover_;

}

void CollectionModel::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (!pending_art_.contains(id)) return;

  ItemAndCacheKey item_and_cache_key = pending_art_.take(id);
  CollectionItem *item = item_and_cache_key.first;
  if (!item) return;

  const QString &cache_key = item_and_cache_key.second;

  pending_cache_keys_.remove(cache_key);

  // Insert this image in the cache.
  if (!result.success || result.image_scaled.isNull() || result.type == AlbumCoverLoaderResult::Type::Unset) {
    // Set the no_cover image so we don't continually try to load art.
    QPixmapCache::insert(cache_key, pixmap_no_cover_);
  }
  else {
    QPixmap image_pixmap;
    image_pixmap = QPixmap::fromImage(result.image_scaled);
    QPixmapCache::insert(cache_key, image_pixmap);
  }

  // If we have a valid cover not already in the disk cache
  if (use_disk_cache_ && sIconCache && result.success && !result.image_scaled.isNull()) {
    const QUrl disk_cache_key = AlbumIconPixmapDiskCacheKey(cache_key);
    ScopedPtr<QIODevice> disk_cache_img(sIconCache->data(disk_cache_key));
    if (!disk_cache_img) {
      QNetworkCacheMetaData disk_cache_metadata;
      disk_cache_metadata.setSaveToDisk(true);
      disk_cache_metadata.setUrl(disk_cache_key);
      // Qt 6 now ignores any entry without headers, so add a fake header.
      disk_cache_metadata.setRawHeaders(QNetworkCacheMetaData::RawHeaderList() << qMakePair(QByteArray("collection-thumbnail"), cache_key.toUtf8()));
      QIODevice *device_iconcache = sIconCache->prepare(disk_cache_metadata);
      if (device_iconcache) {
        result.image_scaled.save(device_iconcache, "XPM");
        sIconCache->insert(device_iconcache);
      }
    }
  }

  const QModelIndex idx = ItemToIndex(item);
  if (!idx.isValid()) return;

  Q_EMIT dataChanged(idx, idx);

}

QString CollectionModel::DisplayText(const GroupBy group_by, const Song &song) {

  switch (group_by) {
    case GroupBy::AlbumArtist:
      return TextOrUnknown(song.effective_albumartist());
    case GroupBy::Artist:
      return TextOrUnknown(song.artist());
    case GroupBy::Album:
      return TextOrUnknown(song.album());
    case GroupBy::AlbumDisc:
      return PrettyAlbumDisc(song.album(), song.disc());
    case GroupBy::YearAlbum:
      return PrettyYearAlbum(song.year(), song.album());
    case GroupBy::YearAlbumDisc:
      return PrettyYearAlbumDisc(song.year(), song.album(), song.disc());
    case GroupBy::OriginalYearAlbum:
      return PrettyYearAlbum(song.effective_originalyear(), song.album());
    case GroupBy::OriginalYearAlbumDisc:
      return PrettyYearAlbumDisc(song.effective_originalyear(), song.album(), song.disc());
    case GroupBy::Disc:
      return PrettyDisc(std::max(0, song.disc()));
    case GroupBy::Year:
      return QString::number(std::max(0, song.year()));
    case GroupBy::OriginalYear:
      return QString::number(std::max(0, song.effective_originalyear()));
    case GroupBy::Genre:
      return TextOrUnknown(song.genre());
    case GroupBy::Composer:
      return TextOrUnknown(song.composer());
    case GroupBy::Performer:
      return TextOrUnknown(song.performer());
    case GroupBy::Grouping:
      return TextOrUnknown(song.grouping());
    case GroupBy::FileType:
      return song.TextForFiletype();
    case GroupBy::Format:
      return PrettyFormat(song);
    case GroupBy::Samplerate:
      return QString::number(std::max(0, song.samplerate()));
    case GroupBy::Bitdepth:
      return QString::number(std::max(0, song.bitdepth()));
    case GroupBy::Bitrate:
      return QString::number(std::max(0, song.bitrate()));
    case GroupBy::None:
    case GroupBy::GroupByCount:
      return song.TitleWithCompilationArtist();
  }

  return QString();

}

QString CollectionModel::TextOrUnknown(const QString &text) {

  if (text.isEmpty()) return tr("Unknown");
  return text;

}

QString CollectionModel::PrettyYearAlbum(const int year, const QString &album) {

  if (year <= 0) return TextOrUnknown(album);
  return QString::number(year) + " - "_L1 + TextOrUnknown(album);

}

QString CollectionModel::PrettyAlbumDisc(const QString &album, const int disc) {

  if (disc <= 0 || Song::AlbumContainsDisc(album)) return TextOrUnknown(album);
  return TextOrUnknown(album) + " - (Disc "_L1 + QString::number(disc) + ")"_L1;

}

QString CollectionModel::PrettyYearAlbumDisc(const int year, const QString &album, const int disc) {

  QString str;

  if (year <= 0) str = TextOrUnknown(album);
  else str = QString::number(year) + " - "_L1 + TextOrUnknown(album);

  if (!Song::AlbumContainsDisc(album) && disc > 0) str += " - (Disc "_L1 + QString::number(disc) + ")"_L1;

  return str;

}

QString CollectionModel::PrettyDisc(const int disc) {

  return "Disc "_L1 + QString::number(std::max(1, disc));

}

QString CollectionModel::PrettyFormat(const Song &song) {

  if (song.samplerate() <= 0) {
    return song.TextForFiletype();
  }

  if (song.bitdepth() <= 0) {
    return QStringLiteral("%1 (%2)").arg(song.TextForFiletype(), QString::number(song.samplerate() / 1000.0, 'G', 5));
  }

  return QStringLiteral("%1 (%2/%3)").arg(song.TextForFiletype(), QString::number(song.samplerate() / 1000.0, 'G', 5)).arg(song.bitdepth());

}

QString CollectionModel::SortText(const GroupBy group_by, const int container_level, const Song &song, const bool sort_skips_articles) {

  switch (group_by) {
    case GroupBy::AlbumArtist:
      return SortTextForArtist(song.effective_albumartist(), sort_skips_articles);
    case GroupBy::Artist:
      return SortTextForArtist(song.artist(), sort_skips_articles);
    case GroupBy::Album:
      return SortTextForArtist(song.album(), sort_skips_articles);
    case GroupBy::AlbumDisc:
      return song.album() + SortTextForNumber(std::max(0, song.disc()));
    case GroupBy::YearAlbum:
      return SortTextForNumber(std::max(0, song.year())) + song.grouping() + song.album();
    case GroupBy::YearAlbumDisc:
      return SortTextForNumber(std::max(0, song.year())) + song.album() + SortTextForNumber(std::max(0, song.disc()));
    case GroupBy::OriginalYearAlbum:
      return SortTextForNumber(std::max(0, song.effective_originalyear())) + song.grouping() + song.album();
    case GroupBy::OriginalYearAlbumDisc:
      return SortTextForNumber(std::max(0, song.effective_originalyear())) + song.album() + SortTextForNumber(std::max(0, song.disc()));
    case GroupBy::Disc:
      return SortTextForNumber(std::max(0, song.disc()));
    case GroupBy::Year:
      return SortTextForNumber(std::max(0, song.year())) + QLatin1Char(' ');
    case GroupBy::OriginalYear:
      return SortTextForNumber(std::max(0, song.effective_originalyear())) + QLatin1Char(' ');
    case GroupBy::Genre:
      return SortTextForArtist(song.genre(), sort_skips_articles);
    case GroupBy::Composer:
      return SortTextForArtist(song.composer(), sort_skips_articles);
    case GroupBy::Performer:
      return SortTextForArtist(song.performer(), sort_skips_articles);
    case GroupBy::Grouping:
      return SortTextForArtist(song.grouping(), sort_skips_articles);
    case GroupBy::FileType:
      return song.TextForFiletype();
    case GroupBy::Format:
      return PrettyFormat(song);
    case GroupBy::Samplerate:
      return SortTextForNumber(std::max(0, song.samplerate())) + QLatin1Char(' ');
    case GroupBy::Bitdepth:
      return SortTextForNumber(std::max(0, song.bitdepth())) + QLatin1Char(' ');
    case GroupBy::Bitrate:
      return SortTextForNumber(std::max(0, song.bitrate())) + QLatin1Char(' ');
    case GroupBy::None:
    case GroupBy::GroupByCount:{
      if (container_level == 1 && !IsAlbumGroupBy(options_active_.group_by[0])) {
        return SortText(song.title());
      }
      return SortTextForSong(song);
    }
  }

  return QString();

}

QString CollectionModel::SortText(QString text) {

  if (text.isEmpty()) {
    text = " unknown"_L1;
  }
  else {
    text = text.toLower();
  }
  static const QRegularExpression regex_not_words(QStringLiteral("[^\\w ]"), QRegularExpression::UseUnicodePropertiesOption);
  text = text.remove(regex_not_words);

  return text;

}

QString CollectionModel::SortTextForArtist(QString artist, const bool skip_articles) {

  artist = SortText(artist);

  if (skip_articles) {
    for (const auto &i : Song::kArticles) {
      if (artist.startsWith(i)) {
        qint64 ilen = i.length();
        artist = artist.right(artist.length() - ilen) + ", "_L1 + i.left(ilen - 1);
        break;
      }
    }
  }

  return artist;

}

QString CollectionModel::SortTextForNumber(const int number) {
  return QStringLiteral("%1").arg(number, 4, 10, QLatin1Char('0'));
}

QString CollectionModel::SortTextForSong(const Song &song) {

  QString ret = QString::number(std::max(0, song.disc()) * 1000 + std::max(0, song.track()));
  ret.prepend(QStringLiteral("0").repeated(6 - ret.length()));
  ret.append(song.url().toString());
  return ret;

}

QString CollectionModel::SortTextForYear(const int year) {

  QString str = QString::number(year);
  return QStringLiteral("0").repeated(qMax(0, 4 - str.length())) + str;

}

QString CollectionModel::SortTextForBitrate(const int bitrate) {

  QString str = QString::number(bitrate);
  return QStringLiteral("0").repeated(qMax(0, 3 - str.length())) + str;

}

bool CollectionModel::IsSongTitleDataChanged(const Song &song1, const Song &song2) {

  return song1.url() != song2.url() ||
         song1.track() != song2.track() ||
         song1.title() != song2.title() ||
         song1.compilation() != song2.compilation() ||
         (song1.compilation() && song1.artist() != song2.artist());

}

QString CollectionModel::ContainerKey(const GroupBy group_by, const Song &song, bool &has_unique_album_identifier) const {

  QString key;

  switch (group_by) {
    case GroupBy::AlbumArtist:
      key = TextOrUnknown(song.effective_albumartist());
      has_unique_album_identifier = true;
      break;
    case GroupBy::Artist:
      key = TextOrUnknown(song.artist());
      has_unique_album_identifier = true;
      break;
    case GroupBy::Album:
      key = TextOrUnknown(song.album());
      if (!song.album_id().isEmpty()) key.append(QLatin1Char('-') + song.album_id());
      if (options_active_.separate_albums_by_grouping && !song.grouping().isEmpty()) key.append(QLatin1Char('-') + song.grouping());
      break;
    case GroupBy::AlbumDisc:
      key = PrettyAlbumDisc(song.album(), song.disc());
      if (!song.album_id().isEmpty()) key.append(QLatin1Char('-') + song.album_id());
      if (options_active_.separate_albums_by_grouping && !song.grouping().isEmpty()) key.append(QLatin1Char('-') + song.grouping());
      break;
    case GroupBy::YearAlbum:
      key = PrettyYearAlbum(song.year(), song.album());
      if (!song.album_id().isEmpty()) key.append(QLatin1Char('-') + song.album_id());
      if (options_active_.separate_albums_by_grouping && !song.grouping().isEmpty()) key.append(QLatin1Char('-') + song.grouping());
      break;
    case GroupBy::YearAlbumDisc:
      key = PrettyYearAlbumDisc(song.year(), song.album(), song.disc());
      if (!song.album_id().isEmpty()) key.append(QLatin1Char('-') + song.album_id());
      if (options_active_.separate_albums_by_grouping && !song.grouping().isEmpty()) key.append(QLatin1Char('-') + song.grouping());
      break;
    case GroupBy::OriginalYearAlbum:
      key = PrettyYearAlbum(song.effective_originalyear(), song.album());
      if (!song.album_id().isEmpty()) key.append(QLatin1Char('-') + song.album_id());
      if (options_active_.separate_albums_by_grouping && !song.grouping().isEmpty()) key.append(QLatin1Char('-') + song.grouping());
      break;
    case GroupBy::OriginalYearAlbumDisc:
      key = PrettyYearAlbumDisc(song.effective_originalyear(), song.album(), song.disc());
      if (!song.album_id().isEmpty()) key.append(QLatin1Char('-') + song.album_id());
      if (options_active_.separate_albums_by_grouping && !song.grouping().isEmpty()) key.append(QLatin1Char('-') + song.grouping());
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
      has_unique_album_identifier = true;
      break;
    case GroupBy::Performer:
      key = TextOrUnknown(song.performer());
      has_unique_album_identifier = true;
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
      key = PrettyFormat(song);
      break;
    case GroupBy::None:
      qLog(Error) << "GroupBy::None";
      break;
    case GroupBy::GroupByCount:
      qLog(Error) << "GroupBy::GroupByCount";
      break;
  }

  // Make sure we distinguish albums by different artists if the parent group by is not including artist.
  if (IsAlbumGroupBy(group_by) && !has_unique_album_identifier && !song.is_compilation() && !song.effective_albumartist().isEmpty()) {
    key.prepend(u'-');
    key.prepend(TextOrUnknown(song.effective_albumartist()));
    has_unique_album_identifier = true;
  }

  return key;

}

QString CollectionModel::DividerKey(const GroupBy group_by, const Song &song, const QString &sort_text) {

  // Items which are to be grouped under the same divider must produce the same divider key.
  // This will only get called for top-level items.

  if (sort_text.isEmpty()) return QString();

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
      QChar c = sort_text[0];
      if (c.isDigit()) return QStringLiteral("0");
      if (c == u' ') return QString();
      if (c.decompositionTag() != QChar::NoDecomposition) {
        QString decomposition = c.decomposition();
        return QChar(decomposition[0]);
      }
      return c;
    }
    case GroupBy::Year:
    case GroupBy::OriginalYear:
      return SortTextForNumber(sort_text.toInt() / 10 * 10);
    case GroupBy::YearAlbum:
    case GroupBy::YearAlbumDisc:
      return SortTextForNumber(song.year());
    case GroupBy::OriginalYearAlbum:
    case GroupBy::OriginalYearAlbumDisc:
      return SortTextForNumber(song.effective_originalyear());
    case GroupBy::Samplerate:
      return SortTextForNumber(song.samplerate());
    case GroupBy::Bitdepth:
      return SortTextForNumber(song.bitdepth());
    case GroupBy::Bitrate:
      return SortTextForNumber(song.bitrate());
    case GroupBy::None:
    case GroupBy::GroupByCount:
      return QString();
  }

  qLog(Error) << "Unknown GroupBy" << group_by << "for item" << sort_text;

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
      if (key == "0"_L1) return QStringLiteral("0-9");
      return key.toUpper();

    case GroupBy::YearAlbum:
    case GroupBy::YearAlbumDisc:
    case GroupBy::OriginalYearAlbum:
    case GroupBy::OriginalYearAlbumDisc:
      if (key == "0000"_L1) return tr("Unknown");
      return key.toUpper();

    case GroupBy::Year:
    case GroupBy::OriginalYear:
      if (key == "0000"_L1) return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy::Samplerate:
    case GroupBy::Bitdepth:
    case GroupBy::Bitrate:
      if (key == "000"_L1) return tr("Unknown");
      return QString::number(key.toInt());  // To remove leading 0s

    case GroupBy::None:
    case GroupBy::GroupByCount:
      break;
  }

  qLog(Error) << "Unknown GroupBy" << group_by << "for divider key" << key;

  return QString();

}

bool CollectionModel::CompareItems(const CollectionItem *a, const CollectionItem *b) const {

  QVariant left = data(a, CollectionModel::Role_SortText);
  QVariant right = data(b, CollectionModel::Role_SortText);

  if (left.metaType().id() == QMetaType::Int) {
    return left.toInt() < right.toInt();
  }
  else {
    return left.toString() < right.toString();
  }

}

qint64 CollectionModel::MaximumCacheSize(Settings *s, const char *size_id, const char *size_unit_id, const qint64 cache_size_default) {

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
    case CollectionItem::Type::Container: {
      QList<CollectionItem*> children = item->children;
      std::sort(children.begin(), children.end(), std::bind(&CollectionModel::CompareItems, this, std::placeholders::_1, std::placeholders::_2));

      for (CollectionItem *child : children) {
        GetChildSongs(child, urls, songs, song_ids);
      }
      break;
    }

    case CollectionItem::Type::Song:
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
  Q_EMIT TotalSongCountUpdated(count);

}

void CollectionModel::TotalArtistCountUpdatedSlot(const int count) {

  total_artist_count_ = count;
  Q_EMIT TotalArtistCountUpdated(count);

}

void CollectionModel::TotalAlbumCountUpdatedSlot(const int count) {

  total_album_count_ = count;
  Q_EMIT TotalAlbumCountUpdated(count);

}

void CollectionModel::ClearDiskCache() {
  if (sIconCache) sIconCache->clear();
}

void CollectionModel::RowsInserted(const QModelIndex &parent, const int first, const int last) {

  SongList songs;
  for (int i = first; i <= last; i++) {
    const QModelIndex idx = index(i, 0, parent);
    if (!idx.isValid()) continue;
    CollectionItem *item = IndexToItem(idx);
    if (!item || item->type != CollectionItem::Type::Song) continue;
    songs << item->metadata;
  }

  if (!songs.isEmpty()) {
    Q_EMIT SongsAdded(songs);
  }

}

void CollectionModel::RowsRemoved(const QModelIndex &parent, const int first, const int last) {

  SongList songs;
  for (int i = first; i <= last; i++) {
    const QModelIndex idx = index(i, 0, parent);
    if (!idx.isValid()) continue;
    CollectionItem *item = IndexToItem(idx);
    if (!item || item->type != CollectionItem::Type::Song) continue;
    songs << item->metadata;
  }

  Q_EMIT SongsRemoved(songs);

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
