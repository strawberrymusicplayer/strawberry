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

#include <utility>
#include <memory>

#include <QObject>
#include <QApplication>
#include <QThread>
#include <QMutex>
#include <QIODevice>
#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSqlDatabase>

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/database.h"
#include "core/logging.h"
#include "core/scopedtransaction.h"
#include "core/song.h"
#include "core/sqlquery.h"
#include "core/sqlrow.h"
#include "collection/collectionbackend.h"
#include "playlistitem.h"
#include "songplaylistitem.h"
#include "playlistbackend.h"
#include "playlistparsers/cueparser.h"
#include "smartplaylists/playlistgenerator.h"

using namespace Qt::StringLiterals;
using std::make_shared;

namespace {
constexpr int kSongTableJoins = 2;
}

PlaylistBackend::PlaylistBackend(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      db_(app_->database()),
      original_thread_(nullptr) {

  setObjectName(QLatin1String(metaObject()->className()));

  original_thread_ = thread();

}

void PlaylistBackend::Close() {

  if (db_) {
    QMutexLocker l(db_->Mutex());
    db_->Close();
  }

}

void PlaylistBackend::ExitAsync() {
  QMetaObject::invokeMethod(this, &PlaylistBackend::Exit, Qt::QueuedConnection);
}

void PlaylistBackend::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());

  moveToThread(original_thread_);
  Q_EMIT ExitFinished();

}

PlaylistBackend::PlaylistList PlaylistBackend::GetAllPlaylists() {
  return GetPlaylists(GetPlaylistsFlags::GetPlaylists_All);
}

PlaylistBackend::PlaylistList PlaylistBackend::GetAllOpenPlaylists() {
  return GetPlaylists(GetPlaylistsFlags::GetPlaylists_OpenInUi);
}

PlaylistBackend::PlaylistList PlaylistBackend::GetAllFavoritePlaylists() {
  return GetPlaylists(GetPlaylistsFlags::GetPlaylists_Favorite);
}

PlaylistBackend::PlaylistList PlaylistBackend::GetPlaylists(const GetPlaylistsFlags flags) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  PlaylistList ret;

  QStringList condition_list;
  if (flags & GetPlaylistsFlags::GetPlaylists_OpenInUi) {
    condition_list << QStringLiteral("ui_order != -1");
  }
  if (flags & GetPlaylistsFlags::GetPlaylists_Favorite) {
    condition_list << QStringLiteral("is_favorite != 0");
  }
  QString condition;
  if (!condition_list.isEmpty()) {
    condition = " WHERE "_L1 + condition_list.join(" OR "_L1);
  }

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT ROWID, name, last_played, special_type, ui_path, is_favorite, dynamic_playlist_type, dynamic_playlist_data, dynamic_playlist_backend FROM playlists ") + condition + QStringLiteral(" ORDER BY ui_order"));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return ret;
  }

  while (q.next()) {
    Playlist p;
    p.id = q.value(0).toInt();
    p.name = q.value(1).toString();
    p.last_played = q.value(2).toInt();
    p.special_type = q.value(3).toString();
    p.ui_path = q.value(4).toString();
    p.favorite = q.value(5).toBool();
    p.dynamic_type = static_cast<PlaylistGenerator::Type>(q.value(6).toInt());
    p.dynamic_data = q.value(7).toByteArray();
    p.dynamic_backend = q.value(8).toString();
    ret << p;
  }

  return ret;

}

PlaylistBackend::Playlist PlaylistBackend::GetPlaylist(const int id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("SELECT ROWID, name, last_played, special_type, ui_path, is_favorite, dynamic_playlist_type, dynamic_playlist_data, dynamic_playlist_backend FROM playlists WHERE ROWID=:id"));

  q.BindValue(QStringLiteral(":id"), id);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return Playlist();
  }

  q.next();

  Playlist p;
  p.id = q.value(0).toInt();
  p.name = q.value(1).toString();
  p.last_played = q.value(2).toInt();
  p.special_type = q.value(3).toString();
  p.ui_path = q.value(4).toString();
  p.favorite = q.value(5).toBool();
  p.dynamic_type = static_cast<PlaylistGenerator::Type>(q.value(6).toInt());
  p.dynamic_data = q.value(7).toByteArray();
  p.dynamic_backend = q.value(8).toString();

  return p;

}

PlaylistItemPtrList PlaylistBackend::GetPlaylistItems(const int playlist) {

  PlaylistItemPtrList playlistitems;

  {

    QMutexLocker l(db_->Mutex());
    QSqlDatabase db(db_->Connect());

    QString query = QStringLiteral("SELECT %1, %2, p.type FROM playlist_items AS p LEFT JOIN songs ON p.collection_id = songs.ROWID WHERE p.playlist = :playlist").arg(Song::JoinSpec(QStringLiteral("songs")), Song::JoinSpec(QStringLiteral("p")));

    SqlQuery q(db);
    // Forward iterations only may be faster
    q.setForwardOnly(true);
    q.prepare(query);
    q.BindValue(QStringLiteral(":playlist"), playlist);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return PlaylistItemPtrList();
    }

    // it's probable that we'll have a few songs associated with the same CUE, so we're caching results of parsing CUEs
    SharedPtr<NewSongFromQueryState> state_ptr = make_shared<NewSongFromQueryState>();
    while (q.next()) {
      playlistitems << NewPlaylistItemFromQuery(SqlRow(q), state_ptr);
    }

  }

  if (QThread::currentThread() != thread() && QThread::currentThread() != qApp->thread()) {
    Close();
  }

  return playlistitems;

}

SongList PlaylistBackend::GetPlaylistSongs(const int playlist) {

  SongList songs;

  {
    QMutexLocker l(db_->Mutex());
    QSqlDatabase db(db_->Connect());

    QString query = QStringLiteral("SELECT %1, %2, p.type FROM playlist_items AS p LEFT JOIN songs ON p.collection_id = songs.ROWID WHERE p.playlist = :playlist").arg(Song::JoinSpec(QStringLiteral("songs")), Song::JoinSpec(QStringLiteral("p")));

    SqlQuery q(db);
    // Forward iterations only may be faster
    q.setForwardOnly(true);
    q.prepare(query);
    q.BindValue(QStringLiteral(":playlist"), playlist);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return SongList();
    }

    // it's probable that we'll have a few songs associated with the same CUE, so we're caching results of parsing CUEs
    SharedPtr<NewSongFromQueryState> state_ptr = make_shared<NewSongFromQueryState>();
    while (q.next()) {
      songs << NewSongFromQuery(SqlRow(q), state_ptr);
    }

  }

  if (QThread::currentThread() != thread() && QThread::currentThread() != qApp->thread()) {
    Close();
  }

  return songs;

}

PlaylistItemPtr PlaylistBackend::NewPlaylistItemFromQuery(const SqlRow &row, SharedPtr<NewSongFromQueryState> state) {

  // The song tables get joined first, plus one each for the song ROWIDs
  const int playlist_row = static_cast<int>(Song::kRowIdColumns.count()) * kSongTableJoins;

  PlaylistItemPtr item(PlaylistItem::NewFromSource(static_cast<Song::Source>(row.value(playlist_row).toInt())));
  if (item) {
    item->InitFromQuery(row);
    return RestoreCueData(item, state);
  }

  return item;

}

Song PlaylistBackend::NewSongFromQuery(const SqlRow &row, SharedPtr<NewSongFromQueryState> state) {

  return NewPlaylistItemFromQuery(row, state)->Metadata();

}

// If song had a CUE and the CUE still exists, the metadata from it will be applied here.

PlaylistItemPtr PlaylistBackend::RestoreCueData(PlaylistItemPtr item, SharedPtr<NewSongFromQueryState> state) {

  // We need collection to run a CueParser; also, this method applies only to file-type PlaylistItems
  if (item->source() != Song::Source::LocalFile) return item;

  CueParser cue_parser(app_->collection_backend());

  Song song = item->Metadata();
  // We're only interested in .cue songs here
  if (!song.has_cue()) return item;

  QString cue_path = song.cue_path();
  // If .cue was deleted - reload the song
  if (!QFile::exists(cue_path)) {
    item->Reload();
    return item;
  }

  SongList song_list;
  {
    QMutexLocker locker(&state->mutex_);

    if (!state->cached_cues_.contains(cue_path)) {
      QFile cue_file(cue_path);
      if (!cue_file.open(QIODevice::ReadOnly)) return item;

      song_list = cue_parser.Load(&cue_file, cue_path, QDir(cue_path.section(u'/', 0, -2)));
      cue_file.close();
      state->cached_cues_[cue_path] = song_list;
    }
    else {
      song_list = state->cached_cues_[cue_path];
    }
  }

  for (const Song &from_list : std::as_const(song_list)) {
    if (from_list.url().toEncoded() == song.url().toEncoded() && from_list.beginning_nanosec() == song.beginning_nanosec()) {
      // We found a matching section; replace the input item with a new one containing CUE metadata
      return make_shared<SongPlaylistItem>(from_list);
    }
  }

  // There's no such section in the related .cue -> reload the song
  item->Reload();

  return item;

}

void PlaylistBackend::SavePlaylistAsync(int playlist, const PlaylistItemPtrList &items, int last_played, PlaylistGeneratorPtr dynamic) {

  QMetaObject::invokeMethod(this, "SavePlaylist", Qt::QueuedConnection, Q_ARG(int, playlist), Q_ARG(PlaylistItemPtrList, items), Q_ARG(int, last_played), Q_ARG(PlaylistGeneratorPtr, dynamic));

}

void PlaylistBackend::SavePlaylist(int playlist, const PlaylistItemPtrList &items, int last_played, PlaylistGeneratorPtr dynamic) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  qLog(Debug) << "Saving playlist" << playlist;

  ScopedTransaction transaction(&db);

  // Clear the existing items in the playlist
  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM playlist_items WHERE playlist = :playlist"));
    q.BindValue(QStringLiteral(":playlist"), playlist);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  // Save the new ones
  for (PlaylistItemPtr item : items) {  // clazy:exclude=range-loop-reference
    SqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO playlist_items (playlist, type, collection_id, ") + Song::kColumnSpec + QStringLiteral(") VALUES (:playlist, :type, :collection_id, ") + Song::kBindSpec + QStringLiteral(")"));
    q.BindValue(QStringLiteral(":playlist"), playlist);
    item->BindToQuery(&q);

    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  // Update the last played track number
  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE playlists SET last_played=:last_played, dynamic_playlist_type=:dynamic_type, dynamic_playlist_data=:dynamic_data, dynamic_playlist_backend=:dynamic_backend WHERE ROWID=:playlist"));
    q.BindValue(QStringLiteral(":last_played"), last_played);
    if (dynamic) {
      q.BindValue(QStringLiteral(":dynamic_type"), static_cast<int>(dynamic->type()));
      q.BindValue(QStringLiteral(":dynamic_data"), dynamic->Save());
      q.BindValue(QStringLiteral(":dynamic_backend"), dynamic->collection()->songs_table());
    }
    else {
      q.BindValue(QStringLiteral(":dynamic_type"), 0);
      q.BindValue(QStringLiteral(":dynamic_data"), QByteArray());
      q.BindValue(QStringLiteral(":dynamic_backend"), QString());
    }
    q.BindValue(QStringLiteral(":playlist"), playlist);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  transaction.Commit();

}

int PlaylistBackend::CreatePlaylist(const QString &name, const QString &special_type) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  SqlQuery q(db);
  q.prepare(QStringLiteral("INSERT INTO playlists (name, special_type) VALUES (:name, :special_type)"));
  q.BindValue(QStringLiteral(":name"), name);
  q.BindValue(QStringLiteral(":special_type"), special_type);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return -1;
  }

  return q.lastInsertId().toInt();

}

void PlaylistBackend::RemovePlaylist(int id) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());

  ScopedTransaction transaction(&db);

  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM playlists WHERE ROWID=:id"));
    q.BindValue(QStringLiteral(":id"), id);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  {
    SqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM playlist_items WHERE playlist=:id"));
    q.BindValue(QStringLiteral(":id"), id);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  transaction.Commit();

}

void PlaylistBackend::RenamePlaylist(const int id, const QString &new_name) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());
  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE playlists SET name=:name WHERE ROWID=:id"));
  q.BindValue(QStringLiteral(":name"), new_name);
  q.BindValue(QStringLiteral(":id"), id);

  if (!q.Exec()) {
    db_->ReportErrors(q);
  }

}

void PlaylistBackend::FavoritePlaylist(const int id, const bool is_favorite) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());
  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE playlists SET is_favorite=:is_favorite WHERE ROWID=:id"));
  q.BindValue(QStringLiteral(":is_favorite"), is_favorite ? 1 : 0);
  q.BindValue(QStringLiteral(":id"), id);

  if (!q.Exec()) {
    db_->ReportErrors(q);
  }

}

void PlaylistBackend::SetPlaylistOrder(const QList<int> &ids) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());
  ScopedTransaction transaction(&db);

  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE playlists SET ui_order=-1"));
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }

  q.prepare(QStringLiteral("UPDATE playlists SET ui_order=:index WHERE ROWID=:id"));
  for (int i = 0; i < ids.count(); ++i) {
    q.BindValue(QStringLiteral(":index"), i);
    q.BindValue(QStringLiteral(":id"), ids[i]);
    if (!q.Exec()) {
      db_->ReportErrors(q);
      return;
    }
  }

  transaction.Commit();

}

void PlaylistBackend::SetPlaylistUiPath(const int id, const QString &path) {

  QMutexLocker l(db_->Mutex());
  QSqlDatabase db(db_->Connect());
  SqlQuery q(db);
  q.prepare(QStringLiteral("UPDATE playlists SET ui_path=:path WHERE ROWID=:id"));

  ScopedTransaction transaction(&db);

  q.BindValue(QStringLiteral(":path"), path);
  q.BindValue(QStringLiteral(":id"), id);
  if (!q.Exec()) {
    db_->ReportErrors(q);
    return;
  }

  transaction.Commit();

}
