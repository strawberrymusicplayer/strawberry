/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QQueue>

#include "includes/shared_ptr.h"
#include "core/simpletreemodel.h"
#include "core/song.h"
#include "covermanager/albumcoverloaderoptions.h"
#include "covermanager/albumcoverloaderresult.h"
#include "collectionmodelupdate.h"
#include "collectionfilteroptions.h"
#include "collectionitem.h"

class QTimer;
class Settings;

class CollectionBackend;
class CollectionDirectoryModel;
class CollectionFilter;
class AlbumCoverLoader;

class CollectionModel : public SimpleTreeModel<CollectionItem> {
  Q_OBJECT

 public:
  explicit CollectionModel(const SharedPtr<CollectionBackend> backend, const SharedPtr<AlbumCoverLoader> albumcover_loader, QObject *parent = nullptr);
  ~CollectionModel() override;

  static const int kPrettyCoverSize;

  enum Role {
    Role_Type = Qt::UserRole + 1,
    Role_ContainerType,
    Role_SortText,
    Role_ContainerKey,
    Role_Artist,
    Role_IsDivider,
    Role_Editable,
    LastRole
  };

  // These values get saved in Settings - don't change them
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

  struct Options {
    Options() : group_by(GroupBy::AlbumArtist, GroupBy::AlbumDisc, GroupBy::None),
                show_dividers(true),
                show_pretty_covers(true),
                show_various_artists(true),
                sort_skip_articles_for_artists(false),
                sort_skip_articles_for_albums(false),
                separate_albums_by_grouping(false) {}

    Grouping group_by;
    bool show_dividers;
    bool show_pretty_covers;
    bool show_various_artists;
    bool sort_skip_articles_for_artists;
    bool sort_skip_articles_for_albums;
    bool separate_albums_by_grouping;
    CollectionFilterOptions filter_options;
  };

  SharedPtr<CollectionBackend> backend() const { return backend_; }
  CollectionFilter *filter() const { return filter_; }

  void Init();
  void Reset();

  void ReloadSettings();

  CollectionDirectoryModel *directory_model() const { return dir_model_; }

  int total_song_count() const { return total_song_count_; }
  int total_artist_count() const { return total_artist_count_; }
  int total_album_count() const { return total_album_count_; }

  quint64 icon_disk_cache_size() { return static_cast<quint64>(icon_disk_cache_->cacheSize()); }

  const CollectionModel::Grouping GetGroupBy() const { return options_current_.group_by; }
  void SetGroupBy(const CollectionModel::Grouping g, const std::optional<bool> separate_albums_by_grouping = std::optional<bool>());

  static bool IsArtistGroupBy(const GroupBy group_by) {
    return group_by == CollectionModel::GroupBy::Artist || group_by == CollectionModel::GroupBy::AlbumArtist;
  }
  static bool IsAlbumGroupBy(const GroupBy group_by) { return group_by == GroupBy::Album || group_by == GroupBy::YearAlbum || group_by == GroupBy::AlbumDisc || group_by == GroupBy::YearAlbumDisc || group_by == GroupBy::OriginalYearAlbum || group_by == GroupBy::OriginalYearAlbumDisc; }

  QMap<QString, CollectionItem*> container_nodes(const int i) { return container_nodes_[i]; }
  QList<CollectionItem*> song_nodes() const { return song_nodes_.values(); }
  int divider_nodes_count() const { return divider_nodes_.count(); }

  // QAbstractItemModel
  QVariant data(const QModelIndex &idx, const int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &idx) const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;

  // Utility functions for manipulating text
  QString DisplayText(const GroupBy group_by, const Song &song);
  static QString TextOrUnknown(const QString &text);
  static QString PrettyYearAlbum(const int year, const QString &album);
  static QString PrettyAlbumDisc(const QString &album, const int disc);
  static QString PrettyYearAlbumDisc(const int year, const QString &album, const int disc);
  static QString PrettyDisc(const int disc);
  static QString PrettyFormat(const Song &song);
  QString SortText(const GroupBy group_by, const Song &song, const bool sort_skip_articles_for_artists, const bool sort_skip_articles_for_albums);
  static QString SortText(QString text);
  static QString SortTextForName(const QString &name, const bool sort_skip_articles);
  static QString SortTextForNumber(const int number);
  static QString SortTextForSong(const Song &song);
  static QString SortTextForYear(const int year);
  static QString SortTextForBitrate(const int bitrate);
  static QString SkipArticles(QString name);
  static bool IsSongTitleDataChanged(const Song &song1, const Song &song2);
  QString ContainerKey(const GroupBy group_by, const Song &song, bool &has_unique_album_identifier) const;

  // Get information about the collection
  void GetChildSongs(CollectionItem *item, SongList &songs, QSet<int> &song_ids, QList<QUrl> &urls) const;
  SongList GetChildSongs(const QList<CollectionItem*> items) const;
  SongList GetChildSongs(CollectionItem *item) const;
  SongList GetChildSongs(const QModelIndex &idx) const;
  SongList GetChildSongs(const QModelIndexList &indexes) const;

  bool CompareItems(CollectionItem *a, CollectionItem *b) const;

  bool HasParentAlbumGroupBy(CollectionItem *item) const;

 Q_SIGNALS:
  void TotalSongCountUpdated(const int count);
  void TotalArtistCountUpdated(const int count);
  void TotalAlbumCountUpdated(const int count);
  void GroupingChanged(const CollectionModel::Grouping g, const bool separate_albums_by_grouping);
  void SongsAdded(const SongList &songs);
  void SongsRemoved(const SongList &songs);

 public Q_SLOTS:
  void SetFilterMode(const CollectionFilterOptions::FilterMode filter_mode);
  void SetFilterMaxAge(const int filter_max_age);

  void AddReAddOrUpdate(const SongList &songs);
  void RemoveSongs(const SongList &songs);

  void ClearIconDiskCache();

 private:
  void Clear();
  void BeginReset();
  void EndReset();

  QVariant data(CollectionItem *item, const int role) const;

  void ScheduleUpdate(const CollectionModelUpdate::Type type, const SongList &songs = SongList());
  void ScheduleAddSongs(const SongList &songs);
  void ScheduleUpdateSongs(const SongList &songs);
  void ScheduleRemoveSongs(const SongList &songs);

  void AddReAddOrUpdateSongsInternal(const SongList &songs);
  void AddSongsInternal(const SongList &songs);
  void UpdateSongsInternal(const SongList &songs);
  void RemoveSongsInternal(const SongList &songs);

  void CreateDividerItem(const QString &divider_key, const QString &display_text, CollectionItem *parent);
  CollectionItem *CreateContainerItem(const GroupBy group_by, const int container_level, const QString &container_key, const Song &song, CollectionItem *parent);
  void CreateSongItem(const Song &song, CollectionItem *parent);
  void SetSongItemData(CollectionItem *item, const Song &song) const;
  CollectionItem *CreateCompilationArtistNode(CollectionItem *parent);

  void LoadSongsFromSqlAsync();
  SongList LoadSongsFromSql(const CollectionFilterOptions &filter_options = CollectionFilterOptions());

  static QString DividerKey(const GroupBy group_by, const Song &song, const QString &sort_text);
  static QString DividerDisplayText(const GroupBy group_by, const QString &key);

  // Helpers
  static bool IsCompilationArtistNode(const CollectionItem *node) { return node == node->parent->compilation_artist_node_; }
  QString AlbumIconPixmapCacheKey(const CollectionItem *item) const;
  static QUrl AlbumIconPixmapDiskCacheKey(const QString &cache_key);
  QVariant AlbumIcon(CollectionItem *item);
  void ClearItemPixmapCache(CollectionItem *item);
  static qint64 MaximumCacheSize(Settings *s, const char *size_id, const char *size_unit_id, const qint64 cache_size_default);

 private Q_SLOTS:
  void ResetInternal();
  void ScheduleReset();
  void ProcessUpdate();
  void LoadSongsFromSqlAsyncFinished();
  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result);

  // From CollectionBackend
  void TotalSongCountUpdatedSlot(const int count);
  void TotalArtistCountUpdatedSlot(const int count);
  void TotalAlbumCountUpdatedSlot(const int count);

  void RowsInserted(const QModelIndex &parent, const int first, const int last);
  void RowsRemoved(const QModelIndex &parent, const int first, const int last);

 private:
  const SharedPtr<CollectionBackend> backend_;
  const SharedPtr<AlbumCoverLoader> albumcover_loader_;
  CollectionDirectoryModel *dir_model_;
  CollectionFilter *filter_;
  QTimer *timer_update_;

  QPixmap pixmap_no_cover_;
  QIcon icon_artist_;

  Options options_current_;
  Options options_active_;

  bool use_disk_cache_;
  AlbumCoverLoaderOptions::Types cover_types_;

  int total_song_count_;
  int total_artist_count_;
  int total_album_count_;

  bool loading_;

  QQueue<CollectionModelUpdate> updates_;

  // Keyed on database ID
  QMap<int, CollectionItem*> song_nodes_;

  // Keyed on whatever the key is for that level - artist, album, year, etc.
  QMap<QString, CollectionItem*> container_nodes_[3];

  // Keyed on a letter, a year, a century, etc.
  QMap<QString, CollectionItem*> divider_nodes_;

  using ItemAndCacheKey = QPair<CollectionItem*, QString>;
  QMap<quint64, ItemAndCacheKey> pending_art_;
  QSet<QString> pending_cache_keys_;

  QNetworkDiskCache *icon_disk_cache_;
};

Q_DECLARE_METATYPE(CollectionModel::Grouping)

QDataStream &operator<<(QDataStream &s, const CollectionModel::Grouping g);
QDataStream &operator>>(QDataStream &s, CollectionModel::Grouping &g);

#endif  // COLLECTIONMODEL_H
