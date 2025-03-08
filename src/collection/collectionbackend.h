/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef COLLECTIONBACKEND_H
#define COLLECTIONBACKEND_H

#include "config.h"

#include <optional>
#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSqlDatabase>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "collectionfilteroptions.h"
#include "collectionquery.h"
#include "collectiondirectory.h"

class QThread;
class TaskManager;
class Database;

class CollectionBackendInterface : public QObject {
  Q_OBJECT

 public:
  explicit CollectionBackendInterface(QObject *parent = nullptr) : QObject(parent) {}

  struct Album {
    Album() : art_embedded(false), art_unset(false), filetype(Song::FileType::Unknown) {}
    Album(const QString &_album_artist, const QString &_album, const bool _art_embedded, const QUrl &_art_automatic, const QUrl &_art_manual, const bool _art_unset, const QList<QUrl> &_urls, const Song::FileType _filetype, const QString &_cue_path)
        : album_artist(_album_artist),
          album(_album),
          art_embedded(_art_embedded),
          art_automatic(_art_automatic),
          art_manual(_art_manual),
          art_unset(_art_unset),
          urls(_urls),
          filetype(_filetype),
          cue_path(_cue_path) {}

    QString album_artist;
    QString album;

    bool art_embedded;
    QUrl art_automatic;
    QUrl art_manual;
    bool art_unset;
    QList<QUrl> urls;
    Song::FileType filetype;
    QString cue_path;
  };
  using AlbumList = QList<Album>;

  virtual QString songs_table() const = 0;

  virtual Song::Source source() const = 0;

  virtual SharedPtr<Database> db() const = 0;

  virtual void GetAllSongsAsync(const int id = 0) = 0;

  // Get a list of directories in the collection.  Emits DirectoriesDiscovered.
  virtual void LoadDirectoriesAsync() = 0;

  virtual void UpdateTotalSongCountAsync() = 0;
  virtual void UpdateTotalArtistCountAsync() = 0;
  virtual void UpdateTotalAlbumCountAsync() = 0;

  virtual SongList FindSongsInDirectory(const int id) = 0;
  virtual SongList SongsWithMissingFingerprint(const int id) = 0;
  virtual SongList SongsWithMissingLoudnessCharacteristics(const int id) = 0;
  virtual CollectionSubdirectoryList SubdirsInDirectory(const int id) = 0;
  virtual CollectionDirectoryList GetAllDirectories() = 0;
  virtual void ChangeDirPath(const int id, const QString &old_path, const QString &new_path) = 0;

  virtual SongList GetAllSongs() = 0;

  virtual QStringList GetAllArtists(const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;
  virtual QStringList GetAllArtistsWithAlbums(const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;
  virtual SongList GetArtistSongs(const QString &effective_albumartist, const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;
  virtual SongList GetAlbumSongs(const QString &effective_albumartist, const QString &album, const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;
  virtual SongList GetSongsByAlbum(const QString &album, const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;

  virtual SongList GetCompilationSongs(const QString &album, const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;

  virtual AlbumList GetAllAlbums(const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;
  virtual AlbumList GetAlbumsByArtist(const QString &artist, const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;
  virtual AlbumList GetCompilationAlbums(const CollectionFilterOptions &opt = CollectionFilterOptions()) = 0;

  virtual void UpdateEmbeddedAlbumArtAsync(const QString &effective_albumartist, const QString &album, const bool art_embedded) = 0;
  virtual void UpdateManualAlbumArtAsync(const QString &effective_albumartist, const QString &album, const QUrl &art_manual) = 0;
  virtual void UnsetAlbumArtAsync(const QString &effective_albumartist, const QString &album) = 0;
  virtual void ClearAlbumArtAsync(const QString &effective_albumartist, const QString &album, const bool art_unset) = 0;

  virtual Album GetAlbumArt(const QString &effective_albumartist, const QString &album) = 0;

  virtual Song GetSongById(const int id) = 0;

  virtual SongList GetSongsByFingerprint(const QString &fingerprint) = 0;

  // Returns all sections of a song with the given filename. If there's just one section the resulting list will have it's size equal to 1.
  virtual SongList GetSongsByUrl(const QUrl &url, const bool unavailable = false) = 0;
  // Returns a section of a song with the given filename and beginning. If the section is not present in collection, returns invalid song.
  // Using default beginning value is suitable when searching for single-section songs.
  virtual Song GetSongByUrl(const QUrl &url, const qint64 beginning = 0) = 0;
  virtual Song GetSongByUrlAndTrack(const QUrl &url, const int track) = 0;

  virtual void AddDirectoryAsync(const QString &path) = 0;
  virtual void RemoveDirectoryAsync(const CollectionDirectory &dir) = 0;
};

class CollectionBackend : public CollectionBackendInterface {
  Q_OBJECT

 public:

  Q_INVOKABLE explicit CollectionBackend(QObject *parent = nullptr);

  ~CollectionBackend();

  void Init(SharedPtr<Database> db, SharedPtr<TaskManager> task_manager, const Song::Source source, const QString &songs_table, const QString &dirs_table = QString(), const QString &subdirs_table = QString());

  void Close();

  void ExitAsync();

  void ReportErrors(const CollectionQuery &query);

  Song::Source source() const override { return source_; }

  SharedPtr<Database> db() const override { return db_; }

  QString songs_table() const override { return songs_table_; }
  QString dirs_table() const { return dirs_table_; }
  QString subdirs_table() const { return subdirs_table_; }

  void GetAllSongsAsync(const int id = 0) override;

  // Get a list of directories in the collection.  Emits DirectoriesDiscovered.
  void LoadDirectoriesAsync() override;

  void UpdateTotalSongCountAsync() override;
  void UpdateTotalArtistCountAsync() override;
  void UpdateTotalAlbumCountAsync() override;

  SongList FindSongsInDirectory(const int id) override;
  SongList SongsWithMissingFingerprint(const int id) override;
  SongList SongsWithMissingLoudnessCharacteristics(const int id) override;
  CollectionSubdirectoryList SubdirsInDirectory(const int id) override;
  CollectionDirectoryList GetAllDirectories() override;
  void ChangeDirPath(const int id, const QString &old_path, const QString &new_path) override;

  SongList GetAllSongs() override;

  QStringList GetAll(const QString &column, const CollectionFilterOptions &filter_options = CollectionFilterOptions());
  QStringList GetAllArtists(const CollectionFilterOptions &opt = CollectionFilterOptions()) override;
  QStringList GetAllArtistsWithAlbums(const CollectionFilterOptions &opt = CollectionFilterOptions()) override;
  SongList GetArtistSongs(const QString &effective_albumartist, const CollectionFilterOptions &opt = CollectionFilterOptions()) override;
  SongList GetAlbumSongs(const QString &effective_albumartist, const QString &album, const CollectionFilterOptions &opt = CollectionFilterOptions()) override;
  SongList GetSongsByAlbum(const QString &album, const CollectionFilterOptions &opt = CollectionFilterOptions()) override;

  SongList GetCompilationSongs(const QString &album, const CollectionFilterOptions &opt = CollectionFilterOptions()) override;

  AlbumList GetAllAlbums(const CollectionFilterOptions &opt = CollectionFilterOptions()) override;
  AlbumList GetCompilationAlbums(const CollectionFilterOptions &opt = CollectionFilterOptions()) override;
  AlbumList GetAlbumsByArtist(const QString &artist, const CollectionFilterOptions &opt = CollectionFilterOptions()) override;

  void UpdateEmbeddedAlbumArtAsync(const QString &effective_albumartist, const QString &album, const bool art_embedded) override;
  void UpdateManualAlbumArtAsync(const QString &effective_albumartist, const QString &album, const QUrl &art_manual) override;
  void UnsetAlbumArtAsync(const QString &effective_albumartist, const QString &album) override;
  void ClearAlbumArtAsync(const QString &effective_albumartist, const QString &album, const bool art_unset) override;

  Album GetAlbumArt(const QString &effective_albumartist, const QString &album) override;

  Song GetSongById(const int id) override;
  SongList GetSongsById(const QList<int> &ids);
  SongList GetSongsById(const QStringList &ids);
  SongList GetSongsByForeignId(const QStringList &ids, const QString &table, const QString &column);

  SongList GetSongsByUrl(const QUrl &url, const bool unavailable = false) override;
  Song GetSongByUrl(const QUrl &url, qint64 beginning = 0) override;
  Song GetSongByUrlAndTrack(const QUrl &url, const int track) override;

  void AddDirectoryAsync(const QString &path) override;
  void RemoveDirectoryAsync(const CollectionDirectory &dir) override;

  bool ExecCollectionQuery(CollectionQuery *query, SongList &songs);
  bool ExecCollectionQuery(CollectionQuery *query, SongMap &songs);

  void IncrementPlayCountAsync(const int id);
  void IncrementSkipCountAsync(const int id, const float progress);
  void ResetPlayStatisticsAsync(const int id, const bool save_tags = false);
  void ResetPlayStatisticsAsync(const QList<int> &id_list, const bool save_tags = false);

  void DeleteAllAsync();

  Song GetSongBySongId(const QString &song_id);
  SongList GetSongsBySongId(const QStringList &song_ids);

  SongList GetSongsByFingerprint(const QString &fingerprint) override;

  SongList ExecuteQuery(const QString &sql);

  void AddOrUpdateSongsAsync(const SongList &songs);
  void UpdateSongsBySongIDAsync(const SongMap &new_songs);

  void UpdateSongRatingAsync(const int id, const float rating, const bool save_tags = false);
  void UpdateSongsRatingAsync(const QList<int> &ids, const float rating, const bool save_tags = false);

  void DeleteSongsAsync(const SongList &songs);
  void DeleteSongsByUrlsAsync(const QList<QUrl> &url);

 public Q_SLOTS:
  void Exit();
  void GetAllSongs(const int id);
  void LoadDirectories();
  void UpdateTotalSongCount();
  void UpdateTotalArtistCount();
  void UpdateTotalAlbumCount();
  void AddDirectory(const QString &path);
  void RemoveDirectory(const CollectionDirectory &dir);
  void AddOrUpdateSongs(const SongList &songs);
  void UpdateSongsBySongID(const SongMap &new_songs);
  void UpdateMTimesOnly(const SongList &songs);
  void DeleteSongs(const SongList &songs);
  void DeleteSongsByUrls(const QList<QUrl> &url);
  void MarkSongsUnavailable(const SongList &songs, const bool unavailable = true);
  void AddOrUpdateSubdirs(const CollectionSubdirectoryList &subdirs);
  void CompilationsNeedUpdating();
  void UpdateEmbeddedAlbumArt(const QString &effective_albumartist, const QString &album, const bool art_embedded);
  void UpdateManualAlbumArt(const QString &effective_albumartist, const QString &album, const QUrl &art_manual);
  void UnsetAlbumArt(const QString &effective_albumartist, const QString &album);
  void ClearAlbumArt(const QString &effective_albumartist, const QString &album, const bool art_unset);
  void ForceCompilation(const QString &album, const QStringList &artists, const bool on);
  void IncrementPlayCount(const int id);
  void IncrementSkipCount(const int id, const float progress);
  void ResetPlayStatistics(const int id, const bool save_tags = false);
  void ResetPlayStatistics(const QList<int> &id_list, const bool save_tags = false);
  bool ResetPlayStatistics(const QStringList &id_str_list);
  void DeleteAll();
  void SongPathChanged(const Song &song, const QFileInfo &new_file, const std::optional<int> new_collection_directory_id);

  SongList GetSongsBy(const QString &artist, const QString &album, const QString &title);
  void UpdateLastPlayed(const QString &artist, const QString &album, const QString &title, const qint64 lastplayed);
  void UpdatePlayCount(const QString &artist, const QString &title, const int playcount, const bool save_tags = false);

  void UpdateSongRating(const int id, const float rating, const bool save_tags = false);
  void UpdateSongsRating(const QList<int> &id_list, const float rating, const bool save_tags = false);

  void UpdateLastSeen(const int directory_id, const int expire_unavailable_songs_days);
  void ExpireSongs(const int directory_id, const int expire_unavailable_songs_days);

 Q_SIGNALS:
  void DirectoryAdded(const CollectionDirectory &dir, const CollectionSubdirectoryList &subdir);
  void DirectoryDeleted(const CollectionDirectory &dir);

  void GotSongs(const SongList &songs, const int id);
  void SongsAdded(const SongList &songs);
  void SongsDeleted(const SongList &songs);
  void SongsChanged(const SongList &songs);
  void SongsStatisticsChanged(const SongList &songs, const bool save_tags = false);

  void DatabaseReset();

  void TotalSongCountUpdated(const int count);
  void TotalArtistCountUpdated(const int count);
  void TotalAlbumCountUpdated(const int count);
  void SongsRatingChanged(const SongList &songs, const bool save_tags);

  void ExitFinished();

  void Error(const QString &error);

 private:
  struct CompilationInfo {
    CompilationInfo() : has_compilation_detected(0), has_not_compilation_detected(0) {}

    QList<QUrl> urls;
    QStringList artists;

    int has_compilation_detected;
    int has_not_compilation_detected;
  };

  bool UpdateCompilations(const QSqlDatabase &db, SongList &changed_songs, const QUrl &url, const bool compilation_detected);
  AlbumList GetAlbums(const QString &artist, const QString &album_artist, const bool compilation_required = false, const CollectionFilterOptions &opt = CollectionFilterOptions());
  AlbumList GetAlbums(const QString &artist, const bool compilation_required, const CollectionFilterOptions &opt = CollectionFilterOptions());
  CollectionSubdirectoryList SubdirsInDirectory(const int id, QSqlDatabase &db);

  Song GetSongById(const int id, QSqlDatabase &db);
  SongList GetSongsById(const QStringList &ids, QSqlDatabase &db);

  Song GetSongBySongId(const QString &song_id, QSqlDatabase &db);
  SongList GetSongsBySongId(const QStringList &song_ids, QSqlDatabase &db);

 private:
  SharedPtr<Database> db_;
  SharedPtr<TaskManager> task_manager_;
  Song::Source source_;
  QString songs_table_;
  QString dirs_table_;
  QString subdirs_table_;
  QThread *original_thread_;
};

#endif  // COLLECTIONBACKEND_H

