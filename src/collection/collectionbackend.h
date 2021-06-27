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

#ifndef COLLECTIONBACKEND_H
#define COLLECTIONBACKEND_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/song.h"
#include "collectionquery.h"
#include "directory.h"

class QThread;
class Database;
class SmartPlaylistSearch;

class CollectionBackendInterface : public QObject {
  Q_OBJECT

 public:
  explicit CollectionBackendInterface(QObject *parent = nullptr) : QObject(parent) {}

  struct Album {
    Album() {}
    Album(const QString &_album_artist, const QString &_album, const QUrl &_art_automatic, const QUrl &_art_manual, const QList<QUrl> &_urls, const Song::FileType _filetype, const QString &_cue_path)
        : album_artist(_album_artist),
          album(_album),
          art_automatic(_art_automatic),
          art_manual(_art_manual),
          urls(_urls),
          filetype(_filetype),
          cue_path(_cue_path) {}

    QString album_artist;
    QString album;

    QUrl art_automatic;
    QUrl art_manual;
    QList<QUrl> urls;
    Song::FileType filetype;
    QString cue_path;
  };
  typedef QList<Album> AlbumList;

  virtual QString songs_table() const = 0;

  virtual Database *db() const = 0;

  virtual void GetAllSongsAsync(const int id = 0) = 0;

  // Get a list of directories in the collection.  Emits DirectoriesDiscovered.
  virtual void LoadDirectoriesAsync() = 0;

  virtual void UpdateTotalSongCountAsync() = 0;
  virtual void UpdateTotalArtistCountAsync() = 0;
  virtual void UpdateTotalAlbumCountAsync() = 0;

  virtual SongList FindSongsInDirectory(const int id) = 0;
  virtual SongList SongsWithMissingFingerprint(const int id) = 0;
  virtual SubdirectoryList SubdirsInDirectory(const int id) = 0;
  virtual DirectoryList GetAllDirectories() = 0;
  virtual void ChangeDirPath(const int id, const QString &old_path, const QString &new_path) = 0;

  virtual QStringList GetAllArtists(const QueryOptions &opt = QueryOptions()) = 0;
  virtual QStringList GetAllArtistsWithAlbums(const QueryOptions &opt = QueryOptions()) = 0;
  virtual SongList GetArtistSongs(const QString &effective_albumartist, const QueryOptions &opt = QueryOptions()) = 0;
  virtual SongList GetAlbumSongs(const QString &effective_albumartist, const QString &album, const QueryOptions &opt = QueryOptions()) = 0;
  virtual SongList GetSongsByAlbum(const QString &album, const QueryOptions &opt = QueryOptions()) = 0;

  virtual SongList GetCompilationSongs(const QString &album, const QueryOptions &opt = QueryOptions()) = 0;

  virtual AlbumList GetAllAlbums(const QueryOptions &opt = QueryOptions()) = 0;
  virtual AlbumList GetAlbumsByArtist(const QString &artist, const QueryOptions &opt = QueryOptions()) = 0;
  virtual AlbumList GetCompilationAlbums(const QueryOptions &opt = QueryOptions()) = 0;

  virtual void UpdateManualAlbumArtAsync(const QString &effective_albumartist, const QString &album, const QUrl &cover_url, const bool clear_art_automatic = false) = 0;
  virtual void UpdateAutomaticAlbumArtAsync(const QString &effective_albumartist, const QString &album, const QUrl &cover_url) = 0;

  virtual Album GetAlbumArt(const QString &effective_albumartist, const QString &album) = 0;

  virtual Song GetSongById(const int id) = 0;

  virtual SongList GetSongsByFingerprint(const QString &fingerprint) = 0;

  // Returns all sections of a song with the given filename. If there's just one section the resulting list will have it's size equal to 1.
  virtual SongList GetSongsByUrl(const QUrl &url, const bool unavailable = false) = 0;
  // Returns a section of a song with the given filename and beginning. If the section is not present in collection, returns invalid song.
  // Using default beginning value is suitable when searching for single-section songs.
  virtual Song GetSongByUrl(const QUrl &url, const qint64 beginning = 0) = 0;

  virtual void AddDirectory(const QString &path) = 0;
  virtual void RemoveDirectory(const Directory &dir) = 0;
};

class CollectionBackend : public CollectionBackendInterface {
  Q_OBJECT

 public:

  Q_INVOKABLE explicit CollectionBackend(QObject *parent = nullptr);

  void Init(Database *db, const Song::Source source, const QString &songs_table, const QString &dirs_table = QString(), const QString &subdirs_table = QString());

  void Close();

  void ExitAsync();

  Database *db() const override { return db_; }

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
  SubdirectoryList SubdirsInDirectory(const int id) override;
  DirectoryList GetAllDirectories() override;
  void ChangeDirPath(const int id, const QString &old_path, const QString &new_path) override;

  QStringList GetAll(const QString &column, const QueryOptions &opt = QueryOptions());
  QStringList GetAllArtists(const QueryOptions &opt = QueryOptions()) override;
  QStringList GetAllArtistsWithAlbums(const QueryOptions &opt = QueryOptions()) override;
  SongList GetArtistSongs(const QString &effective_albumartist, const QueryOptions &opt = QueryOptions()) override;
  SongList GetAlbumSongs(const QString &effective_albumartist, const QString &album, const QueryOptions &opt = QueryOptions()) override;
  SongList GetSongsByAlbum(const QString &album, const QueryOptions &opt = QueryOptions()) override;

  SongList GetCompilationSongs(const QString &album, const QueryOptions &opt = QueryOptions()) override;

  AlbumList GetAllAlbums(const QueryOptions &opt = QueryOptions()) override;
  AlbumList GetCompilationAlbums(const QueryOptions &opt = QueryOptions()) override;
  AlbumList GetAlbumsByArtist(const QString &artist, const QueryOptions &opt = QueryOptions()) override;

  void UpdateManualAlbumArtAsync(const QString &effective_albumartist, const QString &album, const QUrl &cover_url, const bool clear_art_automatic = false) override;
  void UpdateAutomaticAlbumArtAsync(const QString &effective_albumartist, const QString &album, const QUrl &cover_url) override;

  Album GetAlbumArt(const QString &effective_albumartist, const QString &album) override;

  Song GetSongById(const int id) override;
  SongList GetSongsById(const QList<int> &ids);
  SongList GetSongsById(const QStringList &ids);
  SongList GetSongsByForeignId(const QStringList &ids, const QString &table, const QString &column);

  SongList GetSongsByUrl(const QUrl &url, const bool unavailable = false) override;
  Song GetSongByUrl(const QUrl &url, qint64 beginning = 0) override;

  void AddDirectory(const QString &path) override;
  void RemoveDirectory(const Directory &dir) override;

  SongList ExecCollectionQuery(CollectionQuery *query);

  void IncrementPlayCountAsync(const int id);
  void IncrementSkipCountAsync(const int id, const float progress);
  void ResetStatisticsAsync(const int id);

  void DeleteAll();

  Song GetSongBySongId(const QString &song_id);
  SongList GetSongsBySongId(const QStringList &song_ids);

  SongList GetSongsByFingerprint(const QString &fingerprint) override;

  SongList SmartPlaylistsGetAllSongs();
  SongList SmartPlaylistsFindSongs(const SmartPlaylistSearch &search);

  Song::Source Source() const;

  void AddOrUpdateSongsAsync(const SongList &songs);

  void UpdateSongRatingAsync(const int id, const double rating);
  void UpdateSongsRatingAsync(const QList<int> &ids, const double rating);

 public slots:
  void Exit();
  void GetAllSongs(const int id = -1);
  void LoadDirectories();
  void UpdateTotalSongCount();
  void UpdateTotalArtistCount();
  void UpdateTotalAlbumCount();
  void AddOrUpdateSongs(const SongList &songs);
  void UpdateMTimesOnly(const SongList &songs);
  void DeleteSongs(const SongList &songs);
  void MarkSongsUnavailable(const SongList &songs, const bool unavailable = true);
  void AddOrUpdateSubdirs(const SubdirectoryList &subdirs);
  void CompilationsNeedUpdating();
  void UpdateManualAlbumArt(const QString &effective_albumartist, const QString &album, const QUrl &cover_url, const bool clear_art_automatic = false);
  void UpdateAutomaticAlbumArt(const QString &effective_albumartist, const QString &album, const QUrl &cover_url);
  void ForceCompilation(const QString &album, const QList<QString> &artists, const bool on);
  void IncrementPlayCount(const int id);
  void IncrementSkipCount(const int id, const float progress);
  void ResetStatistics(const int id);
  void SongPathChanged(const Song &song, const QFileInfo &new_file);

  SongList GetSongsBy(const QString &artist, const QString &album, const QString &title);
  void UpdateLastPlayed(const QString &artist, const QString &album, const QString &title, const qint64 lastplayed);
  void UpdatePlayCount(const QString &artist, const QString &title, const int playcount);

  void UpdateSongRating(const int id, const double rating);
  void UpdateSongsRating(const QList<int> &id_list, const double rating);

  void UpdateLastSeen(const int directory_id, const int expire_unavailable_songs_days);
  void ExpireSongs(const int directory_id, const int expire_unavailable_songs_days);

 signals:
  void DirectoryDiscovered(Directory, SubdirectoryList);
  void DirectoryDeleted(Directory);

  void GotSongs(SongList, int);
  void SongsDiscovered(SongList);
  void SongsDeleted(SongList);
  void SongsStatisticsChanged(SongList);

  void DatabaseReset();

  void TotalSongCountUpdated(int);
  void TotalArtistCountUpdated(int);
  void TotalAlbumCountUpdated(int);
  void SongsRatingChanged(SongList);

  void ExitFinished();

 private:
  struct CompilationInfo {
    CompilationInfo() : has_compilation_detected(0), has_not_compilation_detected(0) {}

    QList<QUrl> urls;
    QStringList artists;

    int has_compilation_detected;
    int has_not_compilation_detected;
  };

  void UpdateCompilations(QSqlQuery &find_song, QSqlQuery &update_song, SongList &deleted_songs, SongList &added_songs, const QUrl &url, const bool compilation_detected);
  AlbumList GetAlbums(const QString &artist, const QString &album_artist, const bool compilation_required = false, const QueryOptions &opt = QueryOptions());
  AlbumList GetAlbums(const QString &artist, const bool compilation_required, const QueryOptions &opt = QueryOptions());
  SubdirectoryList SubdirsInDirectory(const int id, QSqlDatabase &db);

  Song GetSongById(const int id, QSqlDatabase &db);
  SongList GetSongsById(const QStringList &ids, QSqlDatabase &db);

  Song GetSongBySongId(const QString &song_id, QSqlDatabase &db);
  SongList GetSongsBySongId(const QStringList &song_ids, QSqlDatabase &db);

 private:
  Database *db_;
  Song::Source source_;
  QString songs_table_;
  QString dirs_table_;
  QString subdirs_table_;
  QThread *original_thread_;

};

#endif  // COLLECTIONBACKEND_H

