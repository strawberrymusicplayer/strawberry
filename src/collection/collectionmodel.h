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

#ifndef COLLECTIONMODEL_H
#define COLLECTIONMODEL_H

#include "config.h"

#include <optional>

#include <QtGlobal>
#include <QObject>
#include <QAbstractItemModel>
#include <QFuture>
#include <QDataStream>
#include <QMetaType>
#include <QPair>
#include <QSet>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QImage>
#include <QIcon>
#include <QPixmap>
#include <QNetworkDiskCache>

#include "core/simpletreemodel.h"
#include "core/song.h"
#include "core/sqlrow.h"
#include "covermanager/albumcoverloader.h"
#include "collectionfilteroptions.h"
#include "collectionquery.h"
#include "collectionqueryoptions.h"
#include "collectionitem.h"
#include "covermanager/albumcoverloaderoptions.h"

class QSettings;

class Application;
class CollectionBackend;
class CollectionDirectoryModel;

class CollectionModel : public SimpleTreeModel<CollectionItem> {
  Q_OBJECT

 public:
  explicit CollectionModel(CollectionBackend *backend, Application *app, QObject *parent = nullptr);
  ~CollectionModel() override;

  static const int kPrettyCoverSize;
  static const char *kPixmapDiskCacheDir;

  enum Role {
    Role_Type = Qt::UserRole + 1,
    Role_ContainerType,
    Role_SortText,
    Role_Key,
    Role_Artist,
    Role_IsDivider,
    Role_Editable,
    LastRole
  };

  // These values get saved in QSettings - don't change them
  enum class GroupBy {
    None = 0,
    AlbumArtist = 1,
    Artist = 2,
    Album = 3,
    AlbumDisc = 4,
    YearAlbum = 5,
    YearAlbumDisc = 6,
    OriginalYearAlbum = 7,
    OriginalYearAlbumDisc = 8,
    Disc = 9,
    Year = 10,
    OriginalYear = 11,
    Genre = 12,
    Composer = 13,
    Performer = 14,
    Grouping = 15,
    FileType = 16,
    Format = 17,
    Samplerate = 18,
    Bitdepth = 19,
    Bitrate = 20,
    GroupByCount = 21,
  };
  Q_ENUM(GroupBy)

  struct Grouping {
    explicit Grouping(GroupBy f = GroupBy::None, GroupBy s = GroupBy::None, GroupBy t = GroupBy::None)
        : first(f), second(s), third(t) {}

    GroupBy first;
    GroupBy second;
    GroupBy third;

    const GroupBy &operator[](const int i) const;
    GroupBy &operator[](const int i);
    bool operator==(const Grouping other) const {
      return first == other.first && second == other.second && third == other.third;
    }
    bool operator!=(const Grouping other) const { return !(*this == other); }
  };

  struct QueryResult {
    QueryResult() : create_va(false) {}

    SqlRowList rows;
    bool create_va;
  };

  CollectionBackend *backend() const { return backend_; }
  CollectionDirectoryModel *directory_model() const { return dir_model_; }

  // Call before Init()
  void set_show_various_artists(const bool show_various_artists) { show_various_artists_ = show_various_artists; }

  // Get information about the collection
  void GetChildSongs(CollectionItem *item, QList<QUrl> *urls, SongList *songs, QSet<int> *song_ids) const;
  SongList GetChildSongs(const QModelIndex &idx) const;
  SongList GetChildSongs(const QModelIndexList &indexes) const;

  // Might be accurate
  int total_song_count() const { return total_song_count_; }
  int total_artist_count() const { return total_artist_count_; }
  int total_album_count() const { return total_album_count_; }

  // QAbstractItemModel
  QVariant data(const QModelIndex &idx, const int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &idx) const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  bool canFetchMore(const QModelIndex &parent) const override;

  // Whether or not to use album cover art, if it exists, in the collection view
  void set_pretty_covers(const bool use_pretty_covers);
  bool use_pretty_covers() const { return use_pretty_covers_; }

  // Whether or not to show letters heading in the collection view
  void set_show_dividers(const bool show_dividers);

  // Reload settings.
  void ReloadSettings();

  // Utility functions for manipulating text
  static QString TextOrUnknown(const QString &text);
  static QString PrettyYearAlbum(const int year, const QString &album);
  static QString PrettyAlbumDisc(const QString &album, const int disc);
  static QString PrettyYearAlbumDisc(const int year, const QString &album, const int disc);
  static QString PrettyDisc(const int disc);
  static QString SortText(QString text);
  static QString SortTextForNumber(const int number);
  static QString SortTextForArtist(QString artist);
  static QString SortTextForSong(const Song &song);
  static QString SortTextForYear(const int year);
  static QString SortTextForBitrate(const int bitrate);

  quint64 icon_cache_disk_size() { return sIconCache->cacheSize(); }

  static bool IsArtistGroupBy(const GroupBy group_by) {
    return group_by == CollectionModel::GroupBy::Artist || group_by == CollectionModel::GroupBy::AlbumArtist;
  }
  static bool IsAlbumGroupBy(const GroupBy group_by) { return group_by == GroupBy::Album || group_by == GroupBy::YearAlbum || group_by == GroupBy::AlbumDisc || group_by == GroupBy::YearAlbumDisc || group_by == GroupBy::OriginalYearAlbum || group_by == GroupBy::OriginalYearAlbumDisc; }

  void set_use_lazy_loading(const bool value) { use_lazy_loading_ = value; }

  QMap<QString, CollectionItem*> container_nodes(const int i) { return container_nodes_[i]; }
  QList<CollectionItem*> song_nodes() const { return song_nodes_.values(); }
  int divider_nodes_count() const { return divider_nodes_.count(); }

  void ExpandAll(CollectionItem *item = nullptr) const;

  const CollectionModel::Grouping GetGroupBy() const { return group_by_; }
  void SetGroupBy(const CollectionModel::Grouping g, const std::optional<bool> separate_albums_by_grouping = std::optional<bool>());

  static QString ContainerKey(const GroupBy group_by, const bool separate_albums_by_grouping, const Song &song);

 signals:
  void TotalSongCountUpdated(int count);
  void TotalArtistCountUpdated(int count);
  void TotalAlbumCountUpdated(int count);
  void GroupingChanged(CollectionModel::Grouping g, bool separate_albums_by_grouping);

 public slots:
  void SetFilterMode(CollectionFilterOptions::FilterMode filter_mode);
  void SetFilterAge(const int filter_age);
  void SetFilterText(const QString &filter_text);

  void Init(const bool async = true);
  void Reset();
  void ResetAsync();

  void SongsDiscovered(const SongList &songs);

 protected:
  void LazyPopulate(CollectionItem *item) override { LazyPopulate(item, true); }
  void LazyPopulate(CollectionItem *parent, const bool signal);

 private slots:
  // From CollectionBackend
  void SongsDeleted(const SongList &songs);
  void SongsSlightlyChanged(const SongList &songs);
  void TotalSongCountUpdatedSlot(const int count);
  void TotalArtistCountUpdatedSlot(const int count);
  void TotalAlbumCountUpdatedSlot(const int count);
  static void ClearDiskCache();

  // Called after ResetAsync
  void ResetAsyncQueryFinished();

  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result);

 private:
  // Provides some optimizations for loading the list of items in the root.
  // This gets called a lot when filtering the playlist, so it's nice to be able to do it in a background thread.
  CollectionQueryOptions PrepareQuery(CollectionItem *parent);
  QueryResult RunQuery(const CollectionFilterOptions &filter_options = CollectionFilterOptions(), const CollectionQueryOptions &query_options = CollectionQueryOptions());
  void PostQuery(CollectionItem *parent, const QueryResult &result, const bool signal);

  bool HasCompilations(const QSqlDatabase &db, const CollectionFilterOptions &filter_options, const CollectionQueryOptions &query_options);

  void BeginReset();

  // Functions for working with queries and creating items.
  // When the model is reset or when a node is lazy-loaded the Collection constructs a database query to populate the items.
  // Filters are added for each parent item, restricting the songs returned to a particular album or artist for example.
  static void SetQueryColumnSpec(const GroupBy group_by, const bool separate_albums_by_grouping, CollectionQueryOptions *query_options);
  static void AddQueryWhere(const GroupBy group_by, const bool separate_albums_by_grouping, CollectionItem *item, CollectionQueryOptions *query_options);

  // Items can be created either from a query that's been run to populate a node, or by a spontaneous SongsDiscovered emission from the backend.
  CollectionItem *ItemFromQuery(const GroupBy group_by, const bool separate_albums_by_grouping, const bool signal, const bool create_divider, CollectionItem *parent, const SqlRow &row, const int container_level);
  CollectionItem *ItemFromSong(const GroupBy group_by, const bool separate_albums_by_grouping, const bool signal, const bool create_divider, CollectionItem *parent, const Song &s, const int container_level);

  // The "Various Artists" node is an annoying special case.
  CollectionItem *CreateCompilationArtistNode(const bool signal, CollectionItem *parent);

  // Helpers for ItemFromQuery and ItemFromSong
  CollectionItem *InitItem(const GroupBy group_by, const bool signal, CollectionItem *parent, const int container_level);
  void FinishItem(const GroupBy group_by, const bool signal, const bool create_divider, CollectionItem *parent, CollectionItem *item);

  static QString DividerKey(const GroupBy group_by, CollectionItem *item);
  static QString DividerDisplayText(const GroupBy group_by, const QString &key);

  // Helpers
  static bool IsCompilationArtistNode(const CollectionItem *node) { return node == node->parent->compilation_artist_node_; }
  QString AlbumIconPixmapCacheKey(const QModelIndex &idx) const;
  QVariant AlbumIcon(const QModelIndex &idx);
  QVariant data(const CollectionItem *item, const int role) const;
  bool CompareItems(const CollectionItem *a, const CollectionItem *b) const;
  static qint64 MaximumCacheSize(QSettings *s, const char *size_id, const char *size_unit_id, const qint64 cache_size_default);

 private:
  CollectionBackend *backend_;
  Application *app_;
  CollectionDirectoryModel *dir_model_;
  bool show_various_artists_;

  int total_song_count_;
  int total_artist_count_;
  int total_album_count_;

  CollectionFilterOptions filter_options_;
  Grouping group_by_;
  bool separate_albums_by_grouping_;

  // Keyed on database ID
  QMap<int, CollectionItem*> song_nodes_;

  // Keyed on whatever the key is for that level - artist, album, year, etc.
  QMap<QString, CollectionItem*> container_nodes_[3];

  // Keyed on a letter, a year, a century, etc.
  QMap<QString, CollectionItem*> divider_nodes_;

  QIcon artist_icon_;
  QIcon album_icon_;
  // Used as a generic icon to show when no cover art is found, fixed to the same size as the artwork (32x32)
  QPixmap no_cover_icon_;

  static QNetworkDiskCache *sIconCache;

  int init_task_id_;

  bool use_pretty_covers_;
  bool show_dividers_;
  bool use_disk_cache_;
  bool use_lazy_loading_;

  AlbumCoverLoaderOptions cover_loader_options_;

  using ItemAndCacheKey = QPair<CollectionItem*, QString>;
  QMap<quint64, ItemAndCacheKey> pending_art_;
  QSet<QString> pending_cache_keys_;
};

Q_DECLARE_METATYPE(CollectionModel::Grouping)

QDataStream &operator<<(QDataStream &s, const CollectionModel::Grouping g);
QDataStream &operator>>(QDataStream &s, CollectionModel::Grouping &g);

#endif  // COLLECTIONMODEL_H
