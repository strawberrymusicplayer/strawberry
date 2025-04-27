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

#include "config.h"

#include <optional>
#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QApplication>
#include <QThread>
#include <QMutex>
#include <QSet>
#include <QMap>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/database.h"
#include "core/scopedtransaction.h"
#include "core/song.h"

#include "collectiondirectory.h"
#include "collectionbackend.h"
#include "collectionfilteroptions.h"
#include "collectionquery.h"
#include "collectiontask.h"

using namespace Qt::Literals::StringLiterals;

CollectionBackend::CollectionBackend(QObject *parent)
    : CollectionBackendInterface(parent),
      db_(nullptr),
      task_manager_(nullptr),
      source_(Song::Source::Unknown),
      original_thread_(nullptr) {

  original_thread_ = thread();

}

CollectionBackend::~CollectionBackend() {

  qLog(Debug) << "Collection backend" << this << "deleted";

}

void CollectionBackend::Init(SharedPtr<Database> db, SharedPtr<TaskManager> task_manager, const Song::Source source, const QString &songs_table, const QString &dirs_table, const QString &subdirs_table) {

  setObjectName(source == Song::Source::Collection ? QLatin1String(QObject::metaObject()->className()) : QStringLiteral("%1%2").arg(Song::DescriptionForSource(source), QLatin1String(QObject::metaObject()->className())));

  db_ = db;
  task_manager_ = task_manager;
  source_ = source;
  songs_table_ = songs_table;
  dirs_table_ = dirs_table;
  subdirs_table_ = subdirs_table;

}

void CollectionBackend::Close() {

  if (db_) {
    QMutexLocker l(db_->Mutex());
    db_->Close();
  }

}

void CollectionBackend::ExitAsync() {
  QMetaObject::invokeMethod(this, &CollectionBackend::Exit, Qt::QueuedConnection);
}

void CollectionBackend::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());

  moveToThread(original_thread_);
  Q_EMIT ExitFinished();

}

void CollectionBackend::ReportErrors(const CollectionQuery &query) {

  const QSqlError sql_error = query.lastError();
  if (sql_error.isValid()) {
    qLog(Error) << "Unable to execute collection SQL query:" << sql_error;
    qLog(Error) << "Failed SQL query:" << query.lastQuery();
    qLog(Error) << "Bound SQL values:" << query.boundValues();
    Q_EMIT Error(tr("Unable to execute collection SQL query: %1").arg(sql_error.text()));
    Q_EMIT Error(tr("Failed SQL query: %1").arg(query.lastQuery()));
  }

}

void CollectionBackend::GetAllSongsAsync(const int id) {
  QMetaObject::invokeMethod(this, "GetAllSongs", Qt::QueuedConnection, Q_ARG(int, id));
}

void CollectionBackend::GetAllSongs(const int id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.setForwardOnly(true);
  q.prepare(QStringLiteral("SELECT %1 FROM %2").arg(Song::kRowIdColumnSpec, songs_table_));
  if (!q.exec()) {
    db_->ReportErrors(q);
    Q_EMIT GotSongs(SongList(), id);
    return;
  }

  SongList songs;
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    songs << song;
  }

  Q_EMIT GotSongs(songs, id);

}

void CollectionBackend::LoadDirectoriesAsync() {
  QMetaObject::invokeMethod(this, &CollectionBackend::LoadDirectories, Qt::QueuedConnection);
}

void CollectionBackend::UpdateTotalSongCountAsync() {
  QMetaObject::invokeMethod(this, &CollectionBackend::UpdateTotalSongCount, Qt::QueuedConnection);
}

void CollectionBackend::UpdateTotalArtistCountAsync() {
  QMetaObject::invokeMethod(this, &CollectionBackend::UpdateTotalArtistCount, Qt::QueuedConnection);
}

void CollectionBackend::UpdateTotalAlbumCountAsync() {
  QMetaObject::invokeMethod(this, &CollectionBackend::UpdateTotalAlbumCount, Qt::QueuedConnection);
}

void CollectionBackend::IncrementPlayCountAsync(const int id) {
  QMetaObject::invokeMethod(this, "IncrementPlayCount", Qt::QueuedConnection, Q_ARG(int, id));
}

void CollectionBackend::IncrementSkipCountAsync(const int id, const float progress) {
  QMetaObject::invokeMethod(this, "IncrementSkipCount", Qt::QueuedConnection, Q_ARG(int, id), Q_ARG(float, progress));
}

void CollectionBackend::ResetPlayStatisticsAsync(const int id, const bool save_tags) {
  QMetaObject::invokeMethod(this, "ResetPlayStatistics", Qt::QueuedConnection, Q_ARG(int, id), Q_ARG(bool, save_tags));
}

void CollectionBackend::ResetPlayStatisticsAsync(const QList<int> &id_list, const bool save_tags) {
  QMetaObject::invokeMethod(this, "ResetPlayStatistics", Qt::QueuedConnection, Q_ARG(QList<int>, id_list), Q_ARG(bool, save_tags));
}

void CollectionBackend::LoadDirectories() {

  const CollectionDirectoryList dirs = GetAllDirectories();

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  for (const CollectionDirectory &dir : dirs) {
    Q_EMIT DirectoryAdded(dir, SubdirsInDirectory(dir.id, db));
  }

}

void CollectionBackend::ChangeDirPath(const int id, const QString &old_path, const QString &new_path) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());
  ScopedTransaction t(&db);

  // Do the dirs table
  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET path=:path WHERE ROWID=:id").arg(dirs_table_));
    q.BindValue(u":path"_s, new_path);
    q.BindValue(u":id"_s, id);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  const QByteArray old_url = QUrl::fromLocalFile(old_path).toEncoded();
  const QByteArray new_url = QUrl::fromLocalFile(new_path).toEncoded();

  const qint64 path_len = old_url.length();

  // Do the subdirs table
  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET path=:path || substr(path, %2) WHERE directory=:id").arg(subdirs_table_).arg(path_len));
    q.BindValue(u":path"_s, new_url);
    q.BindValue(u":id"_s, id);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  // Do the songs table
  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET url=:path || substr(url, %2) WHERE directory=:id").arg(songs_table_).arg(path_len));
    q.BindValue(u":path"_s, new_url);
    q.BindValue(u":id"_s, id);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  t.Commit();

}

CollectionDirectoryList CollectionBackend::GetAllDirectories() {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  CollectionDirectoryList ret;

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT ROWID, path FROM %1").arg(dirs_table_));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return ret;
  }

  while (q.next()) {
    CollectionDirectory dir;
    dir.id = q.value(0).toInt();
    dir.path = q.value(1).toString();

    ret << dir;
  }
  return ret;

}

CollectionSubdirectoryList CollectionBackend::SubdirsInDirectory(const int id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db = db_->Connect();
  return SubdirsInDirectory(id, db);

}

CollectionSubdirectoryList CollectionBackend::SubdirsInDirectory(const int id, QSqlDatabase &db) {

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT path, mtime FROM %1 WHERE directory_id = :dir").arg(subdirs_table_));
  q.BindValue(u":dir"_s, id);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return CollectionSubdirectoryList();
  }

  CollectionSubdirectoryList subdirs;
  while (q.next()) {
    CollectionSubdirectory subdir;
    subdir.directory_id = id;
    subdir.path = q.value(0).toString();
    subdir.mtime = q.value(1).toLongLong();
    subdirs << subdir;
  }

  return subdirs;

}

void CollectionBackend::UpdateTotalSongCount() {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT COUNT(*) FROM %1 WHERE unavailable = 0").arg(songs_table_));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }
  if (!q.next()) {
    db_->ReportErrors(q);
    return;
  }

  Q_EMIT TotalSongCountUpdated(q.value(0).toInt());

}

void CollectionBackend::UpdateTotalArtistCount() {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT COUNT(DISTINCT artist) FROM %1 WHERE unavailable = 0").arg(songs_table_));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }
  if (!q.next()) {
    db_->ReportErrors(q);
    return;
  }

  Q_EMIT TotalArtistCountUpdated(q.value(0).toInt());

}

void CollectionBackend::UpdateTotalAlbumCount() {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT COUNT(*) FROM (SELECT DISTINCT effective_albumartist, album FROM %1 WHERE unavailable = 0)").arg(songs_table_));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }
  if (!q.next()) {
    db_->ReportErrors(q);
    return;
  }

  Q_EMIT TotalAlbumCountUpdated(q.value(0).toInt());

}

void CollectionBackend::AddDirectoryAsync(const QString &path) {
  QMetaObject::invokeMethod(this, "AddDirectory", Qt::QueuedConnection, Q_ARG(QString, path));
}

void CollectionBackend::AddDirectory(const QString &path) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("SELECT ROWID FROM %1 WHERE path = :path").arg(dirs_table_));
    q.BindValue(u":path"_s, path);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
    if (q.next()) {
      return;
    }
  }

  SqlQuery q(db);
  q.prepare(QStringLiteral("INSERT INTO %1 (path, subdirs) VALUES (:path, 1)").arg(dirs_table_));
  q.BindValue(u":path"_s, path);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }

  CollectionDirectory dir;
  dir.path = path;
  dir.id = q.lastInsertId().toInt();

  Q_EMIT DirectoryAdded(dir, CollectionSubdirectoryList());

}

void CollectionBackend::RemoveDirectoryAsync(const CollectionDirectory &dir) {
  QMetaObject::invokeMethod(this, "RemoveDirectory", Qt::QueuedConnection, Q_ARG(CollectionDirectory, dir));
}

void CollectionBackend::RemoveDirectory(const CollectionDirectory &dir) {

  // Remove songs first
  DeleteSongs(FindSongsInDirectory(dir.id));

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  ScopedTransaction transaction(&db);

  // Delete the subdirs that were in this directory
  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM %1 WHERE directory_id = :id").arg(subdirs_table_));
    q.BindValue(u":id"_s, dir.id);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  // Now remove the directory itself
  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM %1 WHERE ROWID = :id").arg(dirs_table_));
    q.BindValue(u":id"_s, dir.id);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  transaction.Commit();

  Q_EMIT DirectoryDeleted(dir);

}

SongList CollectionBackend::FindSongsInDirectory(const int id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE directory_id = :directory_id").arg(Song::kRowIdColumnSpec, songs_table_));
  q.BindValue(u":directory_id"_s, id);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }

  SongList ret;
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    ret << song;
  }
  return ret;

}

SongList CollectionBackend::SongsWithMissingFingerprint(const int id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE directory_id = :directory_id AND unavailable = 0 AND (fingerprint IS NULL OR fingerprint = '')").arg(Song::kRowIdColumnSpec, songs_table_));
  q.BindValue(u":directory_id"_s, id);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }

  SongList ret;
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    ret << song;
  }
  return ret;

}

SongList CollectionBackend::SongsWithMissingLoudnessCharacteristics(const int id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE directory_id = :directory_id AND unavailable = 0 AND (ebur128_integrated_loudness_lufs IS NULL OR ebur128_loudness_range_lu IS NULL)").arg(Song::kRowIdColumnSpec, songs_table_));
  q.BindValue(u":directory_id"_s, id);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }

  SongList ret;
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    ret << song;
  }
  return ret;

}

void CollectionBackend::SongPathChanged(const Song &song, const QFileInfo &new_file, const std::optional<int> new_collection_directory_id) {

  // Take a song and update its path
  Song updated_song = song;
  updated_song.set_source(source_);
  updated_song.set_url(QUrl::fromLocalFile(QDir::cleanPath(new_file.filePath())));
  updated_song.set_basefilename(new_file.fileName());
  updated_song.InitArtManual();
  if (updated_song.is_linked_collection_song() && new_collection_directory_id) {
    updated_song.set_directory_id(new_collection_directory_id.value());
  }

  AddOrUpdateSongs(SongList() << updated_song);

}

void CollectionBackend::AddOrUpdateSubdirs(const CollectionSubdirectoryList &subdirs) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  ScopedTransaction transaction(&db);
  for (const CollectionSubdirectory &subdir : subdirs) {
    if (subdir.mtime == 0) {
      // Delete the subdirectory
      SqlQuery q(db);
      q.prepare(QStringLiteral("DELETE FROM %1 WHERE directory_id = :id AND path = :path").arg(subdirs_table_));
      q.BindValue(u":id"_s, subdir.directory_id);
      q.BindValue(u":path"_s, subdir.path);
      if (!q.Exec()) {
        db_->ReportErrors(q);
        return;
      }
    }
    else {
      // See if this subdirectory already exists in the database
      bool exists = false;
      {
        SqlQuery q(db);
        q.prepare(QStringLiteral("SELECT ROWID FROM %1 WHERE directory_id = :id AND path = :path").arg(subdirs_table_));
        q.BindValue(u":id"_s, subdir.directory_id);
        q.BindValue(u":path"_s, subdir.path);
        if (!q.Exec()) {
          db_->ReportErrors(q);
          return;
        }
        exists = q.next();
      }

      if (exists) {
        SqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE %1 SET mtime = :mtime WHERE directory_id = :id AND path = :path").arg(subdirs_table_));
        q.BindValue(u":mtime"_s, subdir.mtime);
        q.BindValue(u":id"_s, subdir.directory_id);
        q.BindValue(u":path"_s, subdir.path);
        if (!q.Exec()) {
          db_->ReportErrors(q);
          return;
        }
      }
      else {
        SqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO %1 (directory_id, path, mtime) VALUES (:id, :path, :mtime)").arg(subdirs_table_));
        q.BindValue(u":id"_s, subdir.directory_id);
        q.BindValue(u":path"_s, subdir.path);
        q.BindValue(u":mtime"_s, subdir.mtime);
        if (!q.Exec()) {
          db_->ReportErrors(q);
          return;
        }
      }
    }
  }

  transaction.Commit();

}

SongList CollectionBackend::GetAllSongs() {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2").arg(Song::kRowIdColumnSpec, songs_table_));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }

  SongList songs;
  while (q.next()) {
    Song song;
    song.InitFromQuery(q, true);
    songs << song;
  }
  return songs;

}

void CollectionBackend::AddOrUpdateSongsAsync(const SongList &songs) {
  QMetaObject::invokeMethod(this, "AddOrUpdateSongs", Qt::QueuedConnection, Q_ARG(SongList, songs));
}

void CollectionBackend::AddOrUpdateSongs(const SongList &songs) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  CollectionTask task(task_manager_, tr("Updating %1 database.").arg(Song::TextForSource(source_)));
  ScopedTransaction transaction(&db);

  SongList added_songs;
  SongList changed_songs;

  for (const Song &song : songs) {

    // Do a sanity check first - make sure the song's directory still exists
    // This is to fix a possible race condition when a directory is removed while CollectionWatcher is scanning it.
    if (!dirs_table_.isEmpty()) {
      SqlQuery check_dir(db);
      check_dir.prepare(QStringLiteral("SELECT ROWID FROM %1 WHERE ROWID = :id").arg(dirs_table_));
      check_dir.BindValue(u":id"_s, song.directory_id());
      if (!check_dir.Exec()) {
        db_->ReportErrors(check_dir);
        return;
      }

      if (!check_dir.next()) continue;

    }

    if (song.id() != -1) {  // This song exists in the DB.

      // Get the previous song data first
      Song old_song(GetSongById(song.id()));
      if (!old_song.is_valid()) continue;

      // Update
      {
        SqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE %1 SET %2 WHERE ROWID = :id").arg(songs_table_, Song::kUpdateSpec));
        song.BindToQuery(&q);
        q.BindValue(u":id"_s, song.id());
        if (!q.Exec()) {
          db_->ReportErrors(q);
          return;
        }
      }

      changed_songs << song;

      continue;

    }
    else if (!song.song_id().isEmpty()) {  // Song has a unique id, check if the song exists.

      // Get the previous song data first
      Song old_song(GetSongBySongId(song.song_id()));

      if (old_song.is_valid() && old_song.id() != -1) {

        Song new_song = song;
        new_song.set_id(old_song.id());

        // Update
        {
          SqlQuery q(db);
          q.prepare(QStringLiteral("UPDATE %1 SET %2 WHERE ROWID = :id").arg(songs_table_, Song::kUpdateSpec));
          new_song.BindToQuery(&q);
          q.BindValue(u":id"_s, new_song.id());
          if (!q.Exec()) {
            db_->ReportErrors(q);
            return;
          }
        }

        changed_songs << new_song;

        continue;
      }
    }

    // Create new song

    int id = -1;
    {  // Insert the row and create a new ID
      SqlQuery q(db);
      q.prepare(QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)").arg(songs_table_, Song::kColumnSpec, Song::kBindSpec));
      song.BindToQuery(&q);
      if (!q.Exec()) {
        db_->ReportErrors(q);
        return;
      }
      // Get the new ID
      id = q.lastInsertId().toInt();
    }

    if (id == -1) return;

    Song song_copy(song);
    song_copy.set_id(id);
    added_songs << song_copy;

  }

  transaction.Commit();

  if (!added_songs.isEmpty()) Q_EMIT SongsAdded(added_songs);
  if (!changed_songs.isEmpty()) Q_EMIT SongsChanged(changed_songs);

  UpdateTotalSongCountAsync();
  UpdateTotalArtistCountAsync();
  UpdateTotalAlbumCountAsync();

}

void CollectionBackend::UpdateSongsBySongIDAsync(const SongMap &new_songs) {
  QMetaObject::invokeMethod(this, "UpdateSongsBySongID", Qt::QueuedConnection, Q_ARG(SongMap, new_songs));
}

void CollectionBackend::UpdateSongsBySongID(const SongMap &new_songs) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  CollectionTask task(task_manager_, tr("Updating %1 database.").arg(Song::TextForSource(source_)));
  ScopedTransaction transaction(&db);

  SongList added_songs;
  SongList changed_songs;
  SongList deleted_songs;

  SongMap old_songs;
  {
    CollectionQuery query(db, songs_table_);
    if (!ExecCollectionQuery(&query, old_songs)) {
      ReportErrors(query);
      return;
    }
  }

  // Add or update songs.
  const QList new_songs_list = new_songs.values();
  for (const Song &new_song : new_songs_list) {
    if (old_songs.contains(new_song.song_id())) {

      Song old_song = old_songs[new_song.song_id()];

      if (!new_song.IsAllMetadataEqual(old_song) || !new_song.IsFingerprintEqual(old_song)) {  // Update existing song.

        {
          SqlQuery q(db);
          q.prepare(QStringLiteral("UPDATE %1 SET %2 WHERE ROWID = :id").arg(songs_table_, Song::kUpdateSpec));
          new_song.BindToQuery(&q);
          q.BindValue(u":id"_s, old_song.id());
          if (!q.Exec()) {
            db_->ReportErrors(q);
            return;
          }
        }

        Song new_song_copy(new_song);
        new_song_copy.set_id(old_song.id());
        changed_songs << new_song_copy;

      }

    }
    else {  // Add new song
      int id = -1;
      {
        SqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)").arg(songs_table_, Song::kColumnSpec, Song::kBindSpec));
        new_song.BindToQuery(&q);
        if (!q.Exec()) {
          db_->ReportErrors(q);
          return;
        }
        // Get the new ID
        id = q.lastInsertId().toInt();
      }

      if (id == -1) return;

      Song new_song_copy(new_song);
      new_song_copy.set_id(id);
      added_songs << new_song_copy;
    }
  }

  // Delete songs
  const QList old_songs_list = old_songs.values();
  for (const Song &old_song : old_songs_list) {
    if (!new_songs.contains(old_song.song_id())) {
      {
        SqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE ROWID = :id").arg(songs_table_));
        q.BindValue(u":id"_s, old_song.id());
        if (!q.Exec()) {
          db_->ReportErrors(q);
          return;
        }
      }
      deleted_songs << old_song;
    }
  }

  transaction.Commit();

  if (!deleted_songs.isEmpty()) Q_EMIT SongsDeleted(deleted_songs);
  if (!added_songs.isEmpty()) Q_EMIT SongsAdded(added_songs);
  if (!changed_songs.isEmpty()) Q_EMIT SongsChanged(changed_songs);

  UpdateTotalSongCountAsync();
  UpdateTotalArtistCountAsync();
  UpdateTotalAlbumCountAsync();

}

void CollectionBackend::UpdateMTimesOnly(const SongList &songs) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  ScopedTransaction transaction(&db);
  for (const Song &song : songs) {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET mtime = :mtime WHERE ROWID = :id").arg(songs_table_));
    q.BindValue(u":mtime"_s, song.mtime());
    q.BindValue(u":id"_s, song.id());
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }
  transaction.Commit();

}

void CollectionBackend::DeleteSongsAsync(const SongList &songs) {
  QMetaObject::invokeMethod(this, "DeleteSongs", Qt::QueuedConnection, Q_ARG(SongList, songs));
}

void CollectionBackend::DeleteSongs(const SongList &songs) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  ScopedTransaction transaction(&db);
  for (const Song &song : songs) {
    SqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM %1 WHERE ROWID = :id").arg(songs_table_));
    q.BindValue(u":id"_s, song.id());
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  transaction.Commit();

  Q_EMIT SongsDeleted(songs);

  UpdateTotalSongCountAsync();
  UpdateTotalArtistCountAsync();
  UpdateTotalAlbumCountAsync();

}

void CollectionBackend::DeleteSongsByUrlsAsync(const QList<QUrl> &urls) {
  QMetaObject::invokeMethod(this, "DeleteSongsByUrl", Qt::QueuedConnection, Q_ARG(QList<QUrl>, urls));
}

void CollectionBackend::DeleteSongsByUrls(const QList<QUrl> &urls) {

  SongList songs;
  songs.reserve(urls.count());
  for (const QUrl &url : urls) {
    songs << GetSongsByUrl(url);
  }

  if (!songs.isEmpty()) {
    DeleteSongs(songs);
  }

}

void CollectionBackend::MarkSongsUnavailable(const SongList &songs, const bool unavailable) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery query(db);
  query.prepare(QStringLiteral("UPDATE %1 SET unavailable = %2 WHERE ROWID = :id").arg(songs_table_).arg(static_cast<int>(unavailable)));

  ScopedTransaction transaction(&db);
  for (const Song &song : songs) {
    query.BindValue(u":id"_s, song.id());
    if (!query.Exec()) {
      db_->ReportErrors(query);
      return;
    }
  }
  transaction.Commit();

  if (unavailable) {
    Q_EMIT SongsDeleted(songs);
  }
  else {
    Q_EMIT SongsAdded(songs);
  }

  UpdateTotalSongCountAsync();
  UpdateTotalArtistCountAsync();
  UpdateTotalAlbumCountAsync();

}

QStringList CollectionBackend::GetAll(const QString &column, const CollectionFilterOptions &filter_options) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  CollectionQuery query(db, songs_table_, filter_options);
  query.SetColumnSpec(u"DISTINCT "_s + column);
  query.AddCompilationRequirement(false);

  if (!query.Exec()) {
    ReportErrors(query);
    return QStringList();
  }

  QStringList ret;
  while (query.Next()) {
    ret << query.Value(0).toString();
  }
  return ret;

}

QStringList CollectionBackend::GetAllArtists(const CollectionFilterOptions &opt) {

  return GetAll(u"artist"_s, opt);

}

QStringList CollectionBackend::GetAllArtistsWithAlbums(const CollectionFilterOptions &opt) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  // Albums with 'albumartist' field set:
  CollectionQuery query(db, songs_table_, opt);
  query.SetColumnSpec(u"DISTINCT albumartist"_s);
  query.AddCompilationRequirement(false);
  query.AddWhere(u"album"_s, ""_L1, u"!="_s);

  // Albums with no 'albumartist' (extract 'artist'):
  CollectionQuery query2(db, songs_table_, opt);
  query2.SetColumnSpec(u"DISTINCT artist"_s);
  query2.AddCompilationRequirement(false);
  query2.AddWhere(u"album"_s, ""_L1, u"!="_s);
  query2.AddWhere(u"albumartist"_s, ""_L1, u"="_s);

  if (!query.Exec()) {
    ReportErrors(query);
    return QStringList();
  }
  if (!query2.Exec()) {
    ReportErrors(query2);
    return QStringList();
  }

  QSet<QString> artists;
  while (query.Next()) {
    artists << query.Value(0).toString();
  }

  while (query2.Next()) {
    artists << query2.Value(0).toString();
  }

  return QStringList(artists.values());

}

CollectionBackend::AlbumList CollectionBackend::GetAllAlbums(const CollectionFilterOptions &opt) {
  return GetAlbums(QString(), false, opt);
}

CollectionBackend::AlbumList CollectionBackend::GetAlbumsByArtist(const QString &artist, const CollectionFilterOptions &opt) {
  return GetAlbums(artist, false, opt);
}

SongList CollectionBackend::GetArtistSongs(const QString &effective_albumartist, const CollectionFilterOptions &opt) {

  QSqlDatabase db(db_->Connect());
  QMutexLocker l(db_->Mutex());

  CollectionQuery query(db, songs_table_, opt);
  query.AddCompilationRequirement(false);
  query.AddWhere(u"effective_albumartist"_s, effective_albumartist);

  SongList songs;
  if (!ExecCollectionQuery(&query, songs)) {
    ReportErrors(query);
  }

  return songs;

}

SongList CollectionBackend::GetAlbumSongs(const QString &effective_albumartist, const QString &album, const CollectionFilterOptions &opt) {

  QSqlDatabase db(db_->Connect());
  QMutexLocker l(db_->Mutex());

  CollectionQuery query(db, songs_table_, opt);
  query.AddCompilationRequirement(false);
  query.AddWhere(u"effective_albumartist"_s, effective_albumartist);
  query.AddWhere(u"album"_s, album);

  SongList songs;
  if (!ExecCollectionQuery(&query, songs)) {
    ReportErrors(query);
  }

  return songs;

}

SongList CollectionBackend::GetSongsByAlbum(const QString &album, const CollectionFilterOptions &opt) {

  QSqlDatabase db(db_->Connect());
  QMutexLocker l(db_->Mutex());

  CollectionQuery query(db, songs_table_, opt);
  query.AddCompilationRequirement(false);
  query.AddWhere(u"album"_s, album);

  SongList songs;
  if (!ExecCollectionQuery(&query, songs)) {
    ReportErrors(query);
  }

  return songs;

}

bool CollectionBackend::ExecCollectionQuery(CollectionQuery *query, SongList &songs) {

  query->SetColumnSpec(u"%songs_table.ROWID, "_s + Song::kColumnSpec);

  if (!query->Exec()) return false;

  while (query->Next()) {
    Song song(source_);
    song.InitFromQuery(*query, true);
    songs << song;
  }
  return true;

}

bool CollectionBackend::ExecCollectionQuery(CollectionQuery *query, SongMap &songs) {

  query->SetColumnSpec(u"%songs_table.ROWID, "_s + Song::kColumnSpec);

  if (!query->Exec()) return false;

  while (query->Next()) {
    Song song(source_);
    song.InitFromQuery(*query, true);
    songs.insert(song.song_id(), song);
  }
  return true;

}

Song CollectionBackend::GetSongById(const int id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());
  return GetSongById(id, db);

}

SongList CollectionBackend::GetSongsById(const QList<int> &ids) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  QStringList str_ids;
  str_ids.reserve(ids.count());
  for (const int id : ids) {
    str_ids << QString::number(id);
  }

  return GetSongsById(str_ids, db);

}

SongList CollectionBackend::GetSongsById(const QStringList &ids) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  return GetSongsById(ids, db);

}

SongList CollectionBackend::GetSongsByForeignId(const QStringList &ids, const QString &table, const QString &column) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  QString in = ids.join(u',');

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %3.ROWID, %2, %3.%4 FROM %3, %1 WHERE %3.%4 IN (in) AND %1.ROWID = %3.ROWID AND unavailable = 0").arg(songs_table_, Song::kColumnSpec, table, column, in));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }

  QList<Song> ret(ids.count());
  while (q.next()) {
    const QString foreign_id = q.value(static_cast<int>(Song::kColumns.count()) + 1).toString();
    const qint64 index = ids.indexOf(foreign_id);
    if (index == -1) continue;

    ret[index].InitFromQuery(q, true);
  }
  return ret.toList();

}

Song CollectionBackend::GetSongById(const int id, QSqlDatabase &db) {

  SongList list = GetSongsById(QStringList() << QString::number(id), db);
  if (list.isEmpty()) return Song();
  return list.first();

}

SongList CollectionBackend::GetSongsById(const QStringList &ids, QSqlDatabase &db) {

  QString in = ids.join(u',');

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE ROWID IN (%3)").arg(Song::kRowIdColumnSpec, songs_table_, in));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }

  SongList ret;
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    ret << song;
  }
  return ret;

}

Song CollectionBackend::GetSongByUrl(const QUrl &url, const qint64 beginning) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE (url = :url1 OR url = :url2 OR url = :url3 OR url = :url4) AND beginning = :beginning AND unavailable = 0").arg(Song::kRowIdColumnSpec, songs_table_));
  q.BindValue(u":url1"_s, url.toString());
  q.BindValue(u":url2"_s, url.toString(QUrl::FullyEncoded));
  q.BindValue(u":url3"_s, url.toEncoded(QUrl::FullyDecoded));
  q.BindValue(u":url4"_s, url.toEncoded(QUrl::FullyEncoded));
  q.BindValue(u":beginning"_s, beginning);

  if (!q.Exec()) {
    db_->ReportErrors(q);
    return Song();
  }

  if (!q.next()) {
    return Song();
  }

  Song song(source_);
  song.InitFromQuery(q, true);

  return song;

}

Song CollectionBackend::GetSongByUrlAndTrack(const QUrl &url, const int track) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE (url = :url1 OR url = :url2 OR url = :url3 OR url = :url4) AND track = :track AND unavailable = 0").arg(Song::kRowIdColumnSpec, songs_table_));
  q.BindValue(u":url1"_s, url.toString());
  q.BindValue(u":url2"_s, url.toString(QUrl::FullyEncoded));
  q.BindValue(u":url3"_s, url.toEncoded(QUrl::FullyDecoded));
  q.BindValue(u":url4"_s, url.toEncoded(QUrl::FullyEncoded));
  q.BindValue(u":track"_s, track);

  if (!q.Exec()) {
    db_->ReportErrors(q);
    return Song();
  }

  if (!q.next()) {
    return Song();
  }

  Song song(source_);
  song.InitFromQuery(q, true);

  return song;

}

SongList CollectionBackend::GetSongsByUrl(const QUrl &url, const bool unavailable) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE (url = :url1 OR url = :url2 OR url = :url3 OR url = :url4) AND unavailable = :unavailable").arg(Song::kRowIdColumnSpec, songs_table_));
  q.BindValue(u":url1"_s, url.toString());
  q.BindValue(u":url2"_s, url.toString(QUrl::FullyEncoded));
  q.BindValue(u":url3"_s, url.toEncoded(QUrl::FullyDecoded));
  q.BindValue(u":url4"_s, url.toEncoded(QUrl::FullyEncoded));
  q.BindValue(u":unavailable"_s, (unavailable ? 1 : 0));

  SongList songs;
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    songs << song;
  }

  return songs;

}


Song CollectionBackend::GetSongBySongId(const QString &song_id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());
  return GetSongBySongId(song_id, db);

}

SongList CollectionBackend::GetSongsBySongId(const QStringList &song_ids) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  return GetSongsBySongId(song_ids, db);

}

Song CollectionBackend::GetSongBySongId(const QString &song_id, QSqlDatabase &db) {

  SongList list = GetSongsBySongId(QStringList() << song_id, db);
  if (list.isEmpty()) return Song();
  return list.first();

}

SongList CollectionBackend::GetSongsBySongId(const QStringList &song_ids, QSqlDatabase &db) {

  QStringList song_ids2;
  song_ids2.reserve(song_ids.count());
  for (const QString &song_id : song_ids) {
    song_ids2 << QLatin1Char('\'') + song_id + QLatin1Char('\'');
  }
  QString in = song_ids2.join(u',');

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE SONG_ID IN (%3)").arg(Song::kRowIdColumnSpec, songs_table_, in));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }

  SongList ret;
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    ret << song;
  }

  return ret;

}

SongList CollectionBackend::GetSongsByFingerprint(const QString &fingerprint) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE fingerprint = :fingerprint").arg(Song::kRowIdColumnSpec, songs_table_));
  q.BindValue(u":fingerprint"_s, fingerprint);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }

  SongList songs;
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    songs << song;
  }

  return songs;

}


CollectionBackend::AlbumList CollectionBackend::GetCompilationAlbums(const CollectionFilterOptions &opt) {
  return GetAlbums(QString(), true, opt);
}

SongList CollectionBackend::GetCompilationSongs(const QString &album, const CollectionFilterOptions &opt) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  CollectionQuery query(db, songs_table_, opt);
  query.SetColumnSpec(u"%songs_table.ROWID, "_s + Song::kColumnSpec);
  query.AddCompilationRequirement(true);
  query.AddWhere(u"album"_s, album);

  if (!query.Exec()) {
    ReportErrors(query);
    return SongList();
  }

  SongList ret;
  while (query.Next()) {
    Song song(source_);
    song.InitFromQuery(query, true);
    ret << song;
  }
  return ret;

}

void CollectionBackend::CompilationsNeedUpdating() {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  // Look for albums that have songs by more than one 'effective album artist' in the same directory

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT effective_albumartist, album, url, compilation_detected FROM %1 WHERE unavailable = 0 ORDER BY album").arg(songs_table_));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }

  QMap<QString, CompilationInfo> compilation_info;
  while (q.next()) {
    QString artist = q.value(0).toString();
    QString album = q.value(1).toString();
    QUrl url = QUrl::fromEncoded(q.value(2).toString().toUtf8());
    bool compilation_detected = q.value(3).toBool();

    // Ignore songs that don't have an album field set
    if (album.isEmpty()) continue;

    // Find the directory the song is in
    QString directory = url.toString(QUrl::PreferLocalFile | QUrl::RemoveFilename);

    CompilationInfo &info = compilation_info[directory + album];
    info.urls << url;
    if (!info.artists.contains(artist)) {
      info.artists << artist;
    }
    if (compilation_detected) info.has_compilation_detected++;
    else info.has_not_compilation_detected++;
  }

  // Now mark the songs that we think are in compilations
  SongList changed_songs;

  ScopedTransaction transaction(&db);

  QMap<QString, CompilationInfo>::const_iterator it = compilation_info.constBegin();
  for (; it != compilation_info.constEnd(); ++it) {
    const CompilationInfo &info = it.value();

    // If there were more than one 'effective album artist' for this album directory, then it's a compilation.

    for (const QUrl &url : info.urls) {
      if (info.artists.count() > 1) {  // This directory+album is a compilation.
        if (info.has_not_compilation_detected > 0) {  // Run updates if any of the songs is not marked as compilations.
          UpdateCompilations(db, changed_songs, url, true);
        }
      }
      else {
        if (info.has_compilation_detected > 0) {
          UpdateCompilations(db, changed_songs, url, false);
        }
      }
    }
  }

  transaction.Commit();

  if (!changed_songs.isEmpty()) {
    Q_EMIT SongsChanged(changed_songs);
  }

}

bool CollectionBackend::UpdateCompilations(const QSqlDatabase &db, SongList &changed_songs, const QUrl &url, const bool compilation_detected) {

  {  // Get song, so we can tell the model its updated
    SqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE (url = :url1 OR url = :url2 OR url = :url3 OR url = :url4) AND unavailable = 0").arg(Song::kRowIdColumnSpec, songs_table_));
    q.BindValue(u":url1"_s, url.toString());
    q.BindValue(u":url2"_s, url.toString(QUrl::FullyEncoded));
    q.BindValue(u":url3"_s, url.toEncoded(QUrl::FullyDecoded));
    q.BindValue(u":url4"_s, url.toEncoded(QUrl::FullyEncoded));
    if (q.Exec()) {
      while (q.next()) {
        Song song(source_);
        song.InitFromQuery(q, true);
        song.set_compilation_detected(compilation_detected);
        changed_songs << song;
      }
    }
    else {
      db_->ReportErrors(q);
      return false;
    }
  }

  // Update the song
  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE %1 SET compilation_detected = :compilation_detected, compilation_effective = ((compilation OR :compilation_detected OR compilation_on) AND NOT compilation_off) + 0 WHERE (url = :url1 OR url = :url2 OR url = :url3 OR url = :url4) AND unavailable = 0").arg(songs_table_));
  q.BindValue(u":compilation_detected"_s, static_cast<int>(compilation_detected));
  q.BindValue(u":url1"_s, url.toString());
  q.BindValue(u":url2"_s, url.toString(QUrl::FullyEncoded));
  q.BindValue(u":url3"_s, url.toEncoded(QUrl::FullyDecoded));
  q.BindValue(u":url4"_s, url.toEncoded(QUrl::FullyEncoded));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return false;
  }

  return true;

}

CollectionBackend::AlbumList CollectionBackend::GetAlbums(const QString &artist, const bool compilation_required, const CollectionFilterOptions &opt) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  CollectionQuery query(db, songs_table_, opt);
  query.SetColumnSpec(u"url, filetype, cue_path, effective_albumartist, album, compilation_effective, art_embedded, art_automatic, art_manual, art_unset"_s);
  query.SetOrderBy(u"effective_albumartist, album, url"_s);

  if (compilation_required) {
    query.AddCompilationRequirement(true);
  }
  else if (!artist.isEmpty()) {
    query.AddCompilationRequirement(false);
    query.AddWhere(u"effective_albumartist"_s, artist);
  }

  if (!query.Exec()) {
    ReportErrors(query);
    return AlbumList();
  }

  QMap<QString, Album> albums;
  while (query.Next()) {

    Album album_info;
    QUrl url = QUrl::fromEncoded(query.Value(0).toByteArray());

    album_info.filetype = static_cast<Song::FileType>(query.Value(1).toInt());
    const QString filetype = Song::TextForFiletype(album_info.filetype);
    album_info.cue_path = query.Value(2).toString();

    const bool is_compilation = query.Value(5).toBool();
    if (!is_compilation) {
      album_info.album_artist = query.Value(3).toString();
    }

    album_info.album = query.Value(4).toString();

    album_info.art_embedded = query.Value(6).toBool();

    const QString art_automatic = query.Value(7).toString();
    static const QRegularExpression regex_url_schema(u"..+:.*"_s);
    if (art_automatic.contains(regex_url_schema)) {
      album_info.art_automatic = QUrl::fromEncoded(art_automatic.toUtf8());
    }
    else {
      album_info.art_automatic = QUrl::fromLocalFile(art_automatic);
    }

    const QString art_manual = query.Value(8).toString();
    if (art_manual.contains(regex_url_schema)) {
      album_info.art_manual = QUrl::fromEncoded(art_manual.toUtf8());
    }
    else {
      album_info.art_manual = QUrl::fromLocalFile(art_manual);
    }

    album_info.art_unset = query.Value(9).toBool();

    QString key;
    if (!album_info.album_artist.isEmpty()) {
      key.append(album_info.album_artist);
    }
    if (!album_info.album.isEmpty()) {
      if (!key.isEmpty()) key.append(u'-');
      key.append(album_info.album);
    }
    if (!filetype.isEmpty()) {
      key.append(filetype);
    }

    if (key.isEmpty()) continue;

    if (albums.contains(key)) {
      albums[key].urls.append(url);
    }
    else {
      album_info.urls << url;
      albums.insert(key, album_info);
    }

  }

  return albums.values();

}

CollectionBackend::Album CollectionBackend::GetAlbumArt(const QString &effective_albumartist, const QString &album) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  Album ret;
  ret.album = album;
  ret.album_artist = effective_albumartist;

  CollectionQuery query(db, songs_table_);
  query.SetColumnSpec(u"url, art_embedded, art_automatic, art_manual, art_unset"_s);
  if (!effective_albumartist.isEmpty()) {
    query.AddWhere(u"effective_albumartist"_s, effective_albumartist);
  }
  query.AddWhere(u"album"_s, album);

  if (!query.Exec()) {
    ReportErrors(query);
    return ret;
  }

  if (query.Next()) {
    ret.urls << QUrl::fromEncoded(query.Value(0).toByteArray());
    ret.art_embedded = query.Value(1).toInt() == 1;
    ret.art_automatic = QUrl::fromEncoded(query.Value(2).toByteArray());
    ret.art_manual = QUrl::fromEncoded(query.Value(3).toByteArray());
    ret.art_unset = query.Value(4).toInt() == 1;
  }

  return ret;

}

void CollectionBackend::UpdateEmbeddedAlbumArtAsync(const QString &effective_albumartist, const QString &album, const bool art_embedded) {

  QMetaObject::invokeMethod(this, "UpdateEmbeddedAlbumArt", Qt::QueuedConnection, Q_ARG(QString, effective_albumartist), Q_ARG(QString, album), Q_ARG(bool, art_embedded));

}

void CollectionBackend::UpdateEmbeddedAlbumArt(const QString &effective_albumartist, const QString &album, const bool art_embedded) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET art_embedded = :art_embedded, art_unset = 0 WHERE effective_albumartist = :effective_albumartist AND album = :album AND unavailable = 0").arg(songs_table_));
    q.BindValue(u":art_embedded"_s, art_embedded ? 1 : 0);
    q.BindValue(u":effective_albumartist"_s, effective_albumartist);
    q.BindValue(u":album"_s, album);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  SongList songs;
  {
    CollectionQuery q(db, songs_table_);
    q.SetColumnSpec(Song::kRowIdColumnSpec);
    q.AddWhere(u"effective_albumartist"_s, effective_albumartist);
    q.AddWhere(u"album"_s, album);
    if (!q.Exec()) {
      ReportErrors(q);
      return;
    }
    while (q.Next()) {
      Song song(source_);
      song.InitFromQuery(q, true);
      songs << song;
    }
  }

  if (!songs.isEmpty()) {
    Q_EMIT SongsChanged(songs);
  }

}

void CollectionBackend::UpdateManualAlbumArtAsync(const QString &effective_albumartist, const QString &album, const QUrl &art_manual) {

  QMetaObject::invokeMethod(this, "UpdateManualAlbumArt", Qt::QueuedConnection, Q_ARG(QString, effective_albumartist), Q_ARG(QString, album), Q_ARG(QUrl, art_manual));

}

void CollectionBackend::UpdateManualAlbumArt(const QString &effective_albumartist, const QString &album, const QUrl &art_manual) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET art_manual = :art_manual, art_unset = 0 WHERE effective_albumartist = :effective_albumartist AND album = :album AND unavailable = 0").arg(songs_table_));
    q.BindValue(u":art_manual"_s, art_manual.isValid() ? art_manual.toString(QUrl::FullyEncoded) : ""_L1);
    q.BindValue(u":effective_albumartist"_s, effective_albumartist);
    q.BindValue(u":album"_s, album);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  SongList songs;
  {
    CollectionQuery q(db, songs_table_);
    q.SetColumnSpec(Song::kRowIdColumnSpec);
    q.AddWhere(u"effective_albumartist"_s, effective_albumartist);
    q.AddWhere(u"album"_s, album);
    if (!q.Exec()) {
      ReportErrors(q);
      return;
    }
    while (q.Next()) {
      Song song(source_);
      song.InitFromQuery(q, true);
      songs << song;
    }
  }

  if (!songs.isEmpty()) {
    Q_EMIT SongsChanged(songs);
  }

}

void CollectionBackend::UnsetAlbumArtAsync(const QString &effective_albumartist, const QString &album) {

  QMetaObject::invokeMethod(this, "UnsetAlbumArt", Qt::QueuedConnection, Q_ARG(QString, effective_albumartist), Q_ARG(QString, album));

}

void CollectionBackend::UnsetAlbumArt(const QString &effective_albumartist, const QString &album) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET art_unset = 1, art_manual = '', art_automatic = '', art_embedded = '' WHERE effective_albumartist = :effective_albumartist AND album = :album AND unavailable = 0").arg(songs_table_));
    q.BindValue(u":effective_albumartist"_s, effective_albumartist);
    q.BindValue(u":album"_s, album);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  SongList songs;
  {
    CollectionQuery q(db, songs_table_);
    q.SetColumnSpec(Song::kRowIdColumnSpec);
    q.AddWhere(u"effective_albumartist"_s, effective_albumartist);
    q.AddWhere(u"album"_s, album);
    if (!q.Exec()) {
      ReportErrors(q);
      return;
    }
    while (q.Next()) {
      Song song(source_);
      song.InitFromQuery(q, true);
      songs << song;
    }
  }

  if (!songs.isEmpty()) {
    Q_EMIT SongsChanged(songs);
  }

}

void CollectionBackend::ClearAlbumArtAsync(const QString &effective_albumartist, const QString &album, const bool unset) {

  QMetaObject::invokeMethod(this, "ClearAlbumArt", Qt::QueuedConnection, Q_ARG(QString, effective_albumartist), Q_ARG(QString, album), Q_ARG(bool, unset));

}

void CollectionBackend::ClearAlbumArt(const QString &effective_albumartist, const QString &album, const bool art_unset) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET art_embedded = 0, art_automatic = '', art_manual = '', art_unset = :art_unset WHERE effective_albumartist = :effective_albumartist AND album = :album AND unavailable = 0").arg(songs_table_));
    q.BindValue(u":art_unset"_s, art_unset ? 1 : 0);
    q.BindValue(u":effective_albumartist"_s, effective_albumartist);
    q.BindValue(u":album"_s, album);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  SongList songs;
  {
    CollectionQuery q(db, songs_table_);
    q.SetColumnSpec(Song::kRowIdColumnSpec);
    q.AddWhere(u"effective_albumartist"_s, effective_albumartist);
    q.AddWhere(u"album"_s, album);
    if (!q.Exec()) {
      ReportErrors(q);
      return;
    }
    while (q.Next()) {
      Song song(source_);
      song.InitFromQuery(q, true);
      songs << song;
    }
  }

  if (!songs.isEmpty()) {
    Q_EMIT SongsChanged(songs);
  }

}

void CollectionBackend::ForceCompilation(const QString &album, const QStringList &artists, const bool on) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());
  SongList songs;

  for (const QString &artist : artists) {

    // Update the songs
    QString sql(QStringLiteral("UPDATE %1 SET compilation_on = :compilation_on, compilation_off = :compilation_off, compilation_effective = ((compilation OR compilation_detected OR :compilation_on) AND NOT :compilation_off) + 0 WHERE album = :album AND unavailable = 0").arg(songs_table_));
    if (!artist.isEmpty()) sql += " AND artist = :artist"_L1;

    SqlQuery q(db);
    q.prepare(sql);
    q.BindValue(u":compilation_on"_s, on ? 1 : 0);
    q.BindValue(u":compilation_off"_s, on ? 0 : 1);
    q.BindValue(u":album"_s, album);
    if (!artist.isEmpty()) q.BindValue(u":artist"_s, artist);

    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }

    // Get the updated songs

    CollectionQuery query(db, songs_table_);
    query.SetColumnSpec(Song::kRowIdColumnSpec);
    query.AddWhere(u"album"_s, album);
    if (!artist.isEmpty()) query.AddWhere(u"artist"_s, artist);

    if (!query.Exec()) {
      ReportErrors(query);
      return;
    }

    while (query.Next()) {
      Song song(source_);
      song.InitFromQuery(query, true);
      songs << song;
    }
  }

  if (!songs.isEmpty()) {
    Q_EMIT SongsChanged(songs);
  }

}

void CollectionBackend::IncrementPlayCount(const int id) {

  if (id == -1) return;

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE %1 SET playcount = playcount + 1, lastplayed = :now WHERE ROWID = :id").arg(songs_table_));
  q.BindValue(u":now"_s, QDateTime::currentSecsSinceEpoch());
  q.BindValue(u":id"_s, id);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }

  Song new_song = GetSongById(id, db);
  Q_EMIT SongsStatisticsChanged(SongList() << new_song);

}

void CollectionBackend::IncrementSkipCount(const int id, const float progress) {

  Q_UNUSED(progress);

  if (id == -1) return;

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE %1 SET skipcount = skipcount + 1 WHERE ROWID = :id").arg(songs_table_));
  q.BindValue(u":id"_s, id);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }

  Song new_song = GetSongById(id, db);
  Q_EMIT SongsStatisticsChanged(SongList() << new_song);

}

void CollectionBackend::ResetPlayStatistics(const int id, const bool save_tags) {

  if (id == -1) return;

  ResetPlayStatistics(QList<int>() << id, save_tags);

}

void CollectionBackend::ResetPlayStatistics(const QList<int> &id_list, const bool save_tags) {

  if (id_list.isEmpty()) return;

  QStringList id_str_list;
  id_str_list.reserve(id_list.count());
  for (const int id : id_list) {
    id_str_list << QString::number(id);
  }

  const bool success = ResetPlayStatistics(id_str_list);
  if (success) {
    const SongList songs = GetSongsById(id_list);
    Q_EMIT SongsStatisticsChanged(songs, save_tags);
  }

}

bool CollectionBackend::ResetPlayStatistics(const QStringList &id_str_list) {

  if (id_str_list.isEmpty()) return false;

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE %1 SET playcount = 0, skipcount = 0, lastplayed = -1 WHERE ROWID IN (:ids)").arg(songs_table_));
  q.BindValue(u":ids"_s, id_str_list.join(u','));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return false;
  }

  return true;

}

void CollectionBackend::DeleteAllAsync() {

  QMetaObject::invokeMethod(this, &CollectionBackend::DeleteAll, Qt::QueuedConnection);

}

void CollectionBackend::DeleteAll() {

  {
    QMutexLocker l(db_->Mutex());
    QSqlDatabase db(db_->Connect());
    ScopedTransaction t(&db);

    {
      SqlQuery q(db);
      q.prepare(u"DELETE FROM "_s + songs_table_);
      if (!q.Exec()) {
        db_->ReportErrors(q);
        return;
      }
    }

    t.Commit();
  }

  Q_EMIT DatabaseReset();

}

SongList CollectionBackend::ExecuteQuery(const QString &sql) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery query(db);
  query.prepare(sql);
  if (!query.Exec()) {
    db_->ReportErrors(query);
    return SongList();
  }

  SongList songs;
  while (query.next()) {
    Song song;
    song.InitFromQuery(query, true);
    songs << song;
  }

  return songs;

}

SongList CollectionBackend::GetSongsBy(const QString &artist, const QString &album, const QString &title) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SongList songs;
  SqlQuery q(db);
  if (album.isEmpty()) {
    q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE artist = :artist COLLATE NOCASE AND title = :title COLLATE NOCASE").arg(Song::kRowIdColumnSpec, songs_table_));
  }
  else {
    q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE artist = :artist COLLATE NOCASE AND album = :album COLLATE NOCASE AND title = :title COLLATE NOCASE").arg(Song::kRowIdColumnSpec, songs_table_));
  }
  q.BindValue(u":artist"_s, artist);
  if (!album.isEmpty()) q.BindValue(u":album"_s, album);
  q.BindValue(u":title"_s, title);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return SongList();
  }
  while (q.next()) {
    Song song(source_);
    song.InitFromQuery(q, true);
    songs << song;
  }

  return songs;

}

void CollectionBackend::UpdateLastPlayed(const QString &artist, const QString &album, const QString &title, const qint64 lastplayed) {

  const SongList songs = GetSongsBy(artist, album, title);
  if (songs.isEmpty()) {
    qLog(Debug) << "Could not find a matching song in the database for" << artist << album << title;
    return;
  }

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  for (const Song &song : songs) {
    if (song.lastplayed() >= lastplayed) {
      continue;
    }
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET lastplayed = :lastplayed WHERE ROWID = :id").arg(songs_table_));
    q.BindValue(u":lastplayed"_s, lastplayed);
    q.BindValue(u":id"_s, song.id());
    if (!q.Exec()) {
      db_->ReportErrors(q);
      continue;
    }
  }

  Q_EMIT SongsStatisticsChanged(SongList() << songs);

}

void CollectionBackend::UpdatePlayCount(const QString &artist, const QString &title, const int playcount, const bool save_tags) {

  const SongList songs = GetSongsBy(artist, QString(), title);
  if (songs.isEmpty()) {
    qLog(Debug) << "Could not find a matching song in the database for" << artist << title;
    return;
  }

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  for (const Song &song : songs) {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET playcount = :playcount WHERE ROWID = :id").arg(songs_table_));
    q.BindValue(u":playcount"_s, playcount);
    q.BindValue(u":id"_s, song.id());
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  Q_EMIT SongsStatisticsChanged(SongList() << songs, save_tags);

}

void CollectionBackend::UpdateSongRating(const int id, const float rating, const bool save_tags) {

  if (id == -1) return;

  UpdateSongsRating(QList<int>() << id, rating, save_tags);

}

void CollectionBackend::UpdateSongsRating(const QList<int> &id_list, const float rating, const bool save_tags) {

  if (id_list.isEmpty()) return;

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  QStringList id_str_list;
  id_str_list.reserve(id_list.count());
  for (int i : id_list) {
    id_str_list << QString::number(i);
  }
  QString ids = id_str_list.join(u',');
  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE %1 SET rating = :rating WHERE ROWID IN (%2)").arg(songs_table_, ids));
  q.BindValue(u":rating"_s, rating);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }

  SongList new_song_list = GetSongsById(id_str_list, db);

  Q_EMIT SongsRatingChanged(new_song_list, save_tags);

}

void CollectionBackend::UpdateSongRatingAsync(const int id, const float rating, const bool save_tags) {
  QMetaObject::invokeMethod(this, "UpdateSongRating", Qt::QueuedConnection, Q_ARG(int, id), Q_ARG(float, rating), Q_ARG(bool, save_tags));
}

void CollectionBackend::UpdateSongsRatingAsync(const QList<int> &ids, const float rating, const bool save_tags) {
  QMetaObject::invokeMethod(this, "UpdateSongsRating", Qt::QueuedConnection, Q_ARG(QList<int>, ids), Q_ARG(float, rating), Q_ARG(bool, save_tags));
}

void CollectionBackend::UpdateLastSeen(const int directory_id, const int expire_unavailable_songs_days) {

  {
    QMutexLocker l(db_->Mutex());
    QSqlDatabase db(db_->Connect());

    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE %1 SET lastseen = :lastseen WHERE directory_id = :directory_id AND unavailable = 0").arg(songs_table_));
    q.BindValue(u":lastseen"_s, QDateTime::currentSecsSinceEpoch());
    q.BindValue(u":directory_id"_s, directory_id);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  if (expire_unavailable_songs_days > 0) ExpireSongs(directory_id, expire_unavailable_songs_days);

}

void CollectionBackend::ExpireSongs(const int directory_id, const int expire_unavailable_songs_days) {

  SongList songs;
  {
    QMutexLocker l(db_->Mutex());
    QSqlDatabase db(db_->Connect());
    SqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM %2 LEFT JOIN playlist_items ON %2.ROWID = playlist_items.collection_id WHERE %2.directory_id = :directory_id AND %2.unavailable = 1 AND %2.lastseen > 0 AND %2.lastseen < :time AND playlist_items.collection_id IS NULL").arg(Song::JoinSpec(songs_table_), songs_table_));
    q.BindValue(u":directory_id"_s, directory_id);
    q.BindValue(u":time"_s, QDateTime::currentSecsSinceEpoch() - (expire_unavailable_songs_days * 86400LL));
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
    while (q.next()) {
      Song song(source_);
      song.InitFromQuery(q, true);
      songs << song;
    }
  }

  if (!songs.isEmpty()) DeleteSongs(songs);

}
