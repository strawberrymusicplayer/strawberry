/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <stdbool.h>

#include <QtGlobal>
#include <QObject>
#include <QFileInfo>
#include <QList>
#include <QVector>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/song.h"
#include "collectionquery.h"
#include "directory.h"

class Database;

class CollectionBackendInterface : public QObject {
  Q_OBJECT

 public:
  CollectionBackendInterface(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~CollectionBackendInterface() {}

  struct Album {
    Album() {}
    Album(const QString &_artist, const QString &_album_artist, const QString &_album_name, const QString &_art_automatic, const QString &_art_manual, const QUrl &_first_url) :
      artist(_artist),
      album_artist(_album_artist),
      album_name(_album_name),
      art_automatic(_art_automatic),
      art_manual(_art_manual),
      first_url(_first_url) {}

    const QString &effective_albumartist() const {
      return album_artist.isEmpty() ? artist : album_artist;
    }

    QString artist;
    QString album_artist;
    QString album_name;

    QString art_automatic;
    QString art_manual;
    QUrl first_url;
  };
  typedef QList<Album> AlbumList;

  virtual QString songs_table() const = 0;

  // Get a list of directories in the collection.  Emits DirectoriesDiscovered.
  virtual void LoadDirectoriesAsync() = 0;

  virtual void UpdateTotalSongCountAsync() = 0;
  virtual void UpdateTotalArtistCountAsync() = 0;
  virtual void UpdateTotalAlbumCountAsync() = 0;

  virtual SongList FindSongsInDirectory(int id) = 0;
  virtual SubdirectoryList SubdirsInDirectory(int id) = 0;
  virtual DirectoryList GetAllDirectories() = 0;
  virtual void ChangeDirPath(int id, const QString &old_path, const QString &new_path) = 0;

  virtual QStringList GetAllArtists(const QueryOptions &opt = QueryOptions()) = 0;
  virtual QStringList GetAllArtistsWithAlbums(const QueryOptions &opt = QueryOptions()) = 0;
  virtual SongList GetSongsByAlbum(const QString &album, const QueryOptions &opt = QueryOptions()) = 0;
  virtual SongList GetSongs(const QString &artist, const QString &album, const QueryOptions &opt = QueryOptions()) = 0;

  virtual SongList GetCompilationSongs(const QString &album, const QueryOptions &opt = QueryOptions()) = 0;

  virtual AlbumList GetAllAlbums(const QueryOptions &opt = QueryOptions()) = 0;
  virtual AlbumList GetAlbumsByArtist(const QString &artist, const QueryOptions &opt = QueryOptions()) = 0;
  virtual AlbumList GetCompilationAlbums(const QueryOptions &opt = QueryOptions()) = 0;

  virtual void UpdateManualAlbumArtAsync(const QString &artist, const QString &albumartist, const QString &album, const QString &art) = 0;
  virtual Album GetAlbumArt(const QString &artist, const QString &albumartist, const QString &album) = 0;

  virtual Song GetSongById(int id) = 0;

  // Returns all sections of a song with the given filename. If there's just one section the resulting list will have it's size equal to 1.
  virtual SongList GetSongsByUrl(const QUrl &url) = 0;
  // Returns a section of a song with the given filename and beginning. If the section is not present in collection, returns invalid song.
  // Using default beginning value is suitable when searching for single-section songs.
  virtual Song GetSongByUrl(const QUrl &url, qint64 beginning = 0) = 0;

  virtual void AddDirectory(const QString &path) = 0;
  virtual void RemoveDirectory(const Directory &dir) = 0;

  virtual bool ExecQuery(CollectionQuery *q) = 0;
};

class CollectionBackend : public CollectionBackendInterface {
  Q_OBJECT

 public:
  static const char *kSettingsGroup;

  Q_INVOKABLE CollectionBackend(QObject *parent = nullptr);
  void Init(Database *db, const QString &songs_table, const QString &dirs_table, const QString &subdirs_table, const QString &fts_table);

  Database *db() const { return db_; }

  QString songs_table() const { return songs_table_; }
  QString dirs_table() const { return dirs_table_; }
  QString subdirs_table() const { return subdirs_table_; }

  // Get a list of directories in the collection.  Emits DirectoriesDiscovered.
  void LoadDirectoriesAsync();

  void UpdateTotalSongCountAsync();
  void UpdateTotalArtistCountAsync();
  void UpdateTotalAlbumCountAsync();

  SongList FindSongsInDirectory(int id);
  SubdirectoryList SubdirsInDirectory(int id);
  DirectoryList GetAllDirectories();
  void ChangeDirPath(int id, const QString &old_path, const QString &new_path);

  QStringList GetAll(const QString &column, const QueryOptions &opt = QueryOptions());
  QStringList GetAllArtists(const QueryOptions &opt = QueryOptions());
  QStringList GetAllArtistsWithAlbums(const QueryOptions &opt = QueryOptions());
  SongList GetSongsByAlbum(const QString &album, const QueryOptions &opt = QueryOptions());
  SongList GetSongs(const QString &artist, const QString &album, const QueryOptions &opt = QueryOptions());

  SongList GetCompilationSongs(const QString &album, const QueryOptions &opt = QueryOptions());

  AlbumList GetAllAlbums(const QueryOptions &opt = QueryOptions());
  AlbumList GetCompilationAlbums(const QueryOptions &opt = QueryOptions());
  AlbumList GetAlbumsByArtist(const QString &artist, const QueryOptions &opt = QueryOptions());

  void UpdateManualAlbumArtAsync(const QString &artist, const QString &albumartist, const QString &album, const QString &art);
  Album GetAlbumArt(const QString &artist, const QString &albumartist, const QString &album);

  Song GetSongById(int id);
  SongList GetSongsById(const QList<int> &ids);
  SongList GetSongsById(const QStringList &ids);
  SongList GetSongsByForeignId(const QStringList &ids, const QString &table, const QString &column);

  SongList GetSongsByUrl(const QUrl &url);
  Song GetSongByUrl(const QUrl &url, qint64 beginning = 0);

  void AddDirectory(const QString &path);
  void RemoveDirectory(const Directory &dir);

  bool ExecQuery(CollectionQuery *q);
  SongList ExecCollectionQuery(CollectionQuery *query);

  void IncrementPlayCountAsync(int id);
  void IncrementSkipCountAsync(int id, float progress);
  void ResetStatisticsAsync(int id);

  void DeleteAll();

  Song GetSongBySongId(int song_id);
  SongList GetSongsBySongId(const QList<int> &song_ids);
  SongList GetSongsBySongId(const QStringList &song_ids);

 public slots:
  void LoadDirectories();
  void UpdateTotalSongCount();
  void UpdateTotalArtistCount();
  void UpdateTotalAlbumCount();
  void AddOrUpdateSongs(const SongList &songs);
  void UpdateMTimesOnly(const SongList &songs);
  void DeleteSongs(const SongList &songs);
  void MarkSongsUnavailable(const SongList &songs, bool unavailable = true);
  void AddOrUpdateSubdirs(const SubdirectoryList &subdirs);
  void UpdateCompilations();
  void UpdateManualAlbumArt(const QString &artist,  const QString &albumartist, const QString &album, const QString &art);
  void ForceCompilation(const QString &album, const QList<QString> &artists, bool on);
  void IncrementPlayCount(int id);
  void IncrementSkipCount(int id, float progress);
  void ResetStatistics(int id);
  void SongPathChanged(const Song &song, const QFileInfo &new_file);

signals:
  void DirectoryDiscovered(const Directory &dir, const SubdirectoryList &subdirs);
  void DirectoryDeleted(const Directory &dir);

  void SongsDiscovered(const SongList &songs);
  void SongsDeleted(const SongList &songs);
  void SongsStatisticsChanged(const SongList& songs);

  void DatabaseReset();

  void TotalSongCountUpdated(int total);
  void TotalArtistCountUpdated(int total);
  void TotalAlbumCountUpdated(int total);

 private:
  struct CompilationInfo {
    CompilationInfo() : has_compilation_detected(false), has_not_compilation_detected(false) {}

    QSet<QString> artists;
    QSet<QString> directories;

    bool has_compilation_detected;
    bool has_not_compilation_detected;
  };

  void UpdateCompilations(QSqlQuery &find_songs, QSqlQuery &update, SongList &deleted_songs, SongList &added_songs, const QString &album, int compilation_detected);
  AlbumList GetAlbums(const QString &artist, const QString &album_artist, bool compilation = false, const QueryOptions &opt = QueryOptions());
  AlbumList GetAlbums(const QString &artist, bool compilation, const QueryOptions &opt = QueryOptions());
  SubdirectoryList SubdirsInDirectory(int id, QSqlDatabase &db);

  Song GetSongById(int id, QSqlDatabase &db);
  SongList GetSongsById(const QStringList &ids, QSqlDatabase &db);

  Song GetSongBySongId(int song_id, QSqlDatabase &db);
  SongList GetSongsBySongId(const QStringList &song_ids, QSqlDatabase &db);

 private:
  Database *db_;
  QString songs_table_;
  QString dirs_table_;
  QString subdirs_table_;
  QString fts_table_;

};

#endif  // COLLECTIONBACKEND_H

