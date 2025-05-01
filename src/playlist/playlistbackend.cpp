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

#include "includes/shared_ptr.h"
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

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {
constexpr int kSongTableJoins = 2;
}

PlaylistBackend::PlaylistBackend(const SharedPtr<Database> database,
                                 const SharedPtr<TagReaderClient> tagreader_client,
                                 const SharedPtr<CollectionBackend> collection_backend,
                                 QObject *parent)
    : QObject(parent),
      database_(database),
      tagreader_client_(tagreader_client),
      collection_backend_(collection_backend),
      original_thread_(nullptr) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  original_thread_ = thread();

}

void PlaylistBackend::Close() {

  if (database_) {
    QMutexLocker l(database_->Mutex());
    database_->Close();
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

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());

  PlaylistList ret;

  QStringList condition_list;
  if (flags & GetPlaylistsFlags::GetPlaylists_OpenInUi) {
    condition_list << u"ui_order != -1"_s;
  }
  if (flags & GetPlaylistsFlags::GetPlaylists_Favorite) {
    condition_list << u"is_favorite != 0"_s;
  }
  QString condition;
  if (!condition_list.isEmpty()) {
    condition = " WHERE "_L1 + condition_list.join(" OR "_L1);
  }

  SqlQuery q(db);
  q.prepare(u"SELECT ROWID, name, last_played, special_type, ui_path, is_favorite, dynamic_playlist_type, dynamic_playlist_data, dynamic_playlist_backend FROM playlists "_s + condition + u" ORDER BY ui_order"_s);
  if (!q.Exec()) {
    database_->ReportErrors(q);
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

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());

  SqlQuery q(db);
  q.prepare(u"SELECT ROWID, name, last_played, special_type, ui_path, is_favorite, dynamic_playlist_type, dynamic_playlist_data, dynamic_playlist_backend FROM playlists WHERE ROWID=:id"_s);

  q.BindValue(u":id"_s, id);
  if (!q.Exec()) {
    database_->ReportErrors(q);
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

QString PlaylistBackend::PlaylistItemsQuery() {

  return QStringLiteral("SELECT %1, %2, p.type FROM playlist_items AS p "
                        "LEFT JOIN songs ON p.type = songs.source AND p.collection_id = songs.ROWID "
                        "WHERE p.playlist = :playlist"
                        ).arg(Song::JoinSpec(u"songs"_s),
                              Song::JoinSpec(u"p"_s));

}

PlaylistItemPtrList PlaylistBackend::GetPlaylistItems(const int playlist) {

  PlaylistItemPtrList playlist_items;

  {

    QMutexLocker l(database_->Mutex());
    QSqlDatabase db(database_->Connect());
    SqlQuery q(db);
    // Forward iterations only may be faster
    q.setForwardOnly(true);
    q.prepare(PlaylistItemsQuery());
    q.BindValue(u":playlist"_s, playlist);
    if (!q.Exec()) {
      database_->ReportErrors(q);
      return PlaylistItemPtrList();
    }

    // It's probable that we'll have a few songs associated with the same CUE, so we're caching results of parsing CUEs
    SharedPtr<NewSongFromQueryState> state_ptr = make_shared<NewSongFromQueryState>();
    while (q.next()) {
      playlist_items << NewPlaylistItemFromQuery(SqlRow(q), state_ptr);
    }

  }

  if (QThread::currentThread() != thread() && QThread::currentThread() != qApp->thread()) {
    Close();
  }

  return playlist_items;

}

SongList PlaylistBackend::GetPlaylistSongs(const int playlist) {

  SongList songs;

  {
    QMutexLocker l(database_->Mutex());
    QSqlDatabase db(database_->Connect());
    SqlQuery q(db);
    // Forward iterations only may be faster
    q.setForwardOnly(true);
    q.prepare(PlaylistItemsQuery());
    q.BindValue(u":playlist"_s, playlist);
    if (!q.Exec()) {
      database_->ReportErrors(q);
      return SongList();
    }

    // It's probable that we'll have a few songs associated with the same CUE, so we're caching results of parsing CUEs
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

  // The song tables get joined first
  const int playlist_row = static_cast<int>(Song::kRowIdColumns.count()) * kSongTableJoins;
  PlaylistItemPtr item = PlaylistItem::NewFromSource(static_cast<Song::Source>(row.value(playlist_row).toInt()));
  item->InitFromQuery(row);
  return RestoreCueData(item, state);

}

Song PlaylistBackend::NewSongFromQuery(const SqlRow &row, SharedPtr<NewSongFromQueryState> state) {

  return NewPlaylistItemFromQuery(row, state)->EffectiveMetadata();

}

// If song had a CUE and the CUE still exists, the metadata from it will be applied here.

PlaylistItemPtr PlaylistBackend::RestoreCueData(PlaylistItemPtr item, SharedPtr<NewSongFromQueryState> state) {

  // We need collection to run a CueParser; also, this method applies only to file-type PlaylistItems
  if (item->source() != Song::Source::LocalFile) return item;

  CueParser cue_parser(tagreader_client_, collection_backend_);

  Song song = item->EffectiveMetadata();
  // We're only interested in .cue songs here
  if (!song.has_cue()) return item;

  QString cue_path = song.cue_path();
  // If .cue was deleted - reload the song
  if (!QFile::exists(cue_path)) {
    item->Reload();
    return item;
  }

  SongList songs;
  {
    QMutexLocker locker(&state->mutex_);

    if (!state->cached_cues_.contains(cue_path)) {
      QFile cue_file(cue_path);
      if (!cue_file.open(QIODevice::ReadOnly)) return item;

      songs = cue_parser.Load(&cue_file, cue_path, QDir(cue_path.section(u'/', 0, -2))).songs;
      cue_file.close();
      state->cached_cues_[cue_path] = songs;
    }
    else {
      songs = state->cached_cues_[cue_path];
    }
  }

  for (const Song &from_list : std::as_const(songs)) {
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

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());

  qLog(Debug) << "Saving playlist" << playlist;

  ScopedTransaction transaction(&db);

  // Clear the existing items in the playlist
  {
    SqlQuery q(db);
    q.prepare(u"DELETE FROM playlist_items WHERE playlist = :playlist"_s);
    q.BindValue(u":playlist"_s, playlist);
    if (!q.Exec()) {
      database_->ReportErrors(q);
      return;
    }
  }

  // Save the new ones
  for (PlaylistItemPtr item : items) {  // clazy:exclude=range-loop-reference
    SqlQuery q(db);
    q.prepare(u"INSERT INTO playlist_items (playlist, type, collection_id, "_s + Song::kColumnSpec + u") VALUES (:playlist, :type, :collection_id, "_s + Song::kBindSpec + u")"_s);
    q.BindValue(u":playlist"_s, playlist);
    item->BindToQuery(&q);

    if (!q.Exec()) {
      database_->ReportErrors(q);
      return;
    }
  }

  // Update the last played track number
  {
    SqlQuery q(db);
    q.prepare(u"UPDATE playlists SET last_played=:last_played, dynamic_playlist_type=:dynamic_type, dynamic_playlist_data=:dynamic_data, dynamic_playlist_backend=:dynamic_backend WHERE ROWID=:playlist"_s);
    q.BindValue(u":last_played"_s, last_played);
    if (dynamic) {
      q.BindValue(u":dynamic_type"_s, static_cast<int>(dynamic->type()));
      q.BindValue(u":dynamic_data"_s, dynamic->Save());
      q.BindValue(u":dynamic_backend"_s, dynamic->collection()->songs_table());
    }
    else {
      q.BindValue(u":dynamic_type"_s, 0);
      q.BindValue(u":dynamic_data"_s, QByteArray());
      q.BindValue(u":dynamic_backend"_s, QString());
    }
    q.BindValue(u":playlist"_s, playlist);
    if (!q.Exec()) {
      database_->ReportErrors(q);
      return;
    }
  }

  transaction.Commit();

}

int PlaylistBackend::CreatePlaylist(const QString &name, const QString &special_type) {

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());

  SqlQuery q(db);
  q.prepare(u"INSERT INTO playlists (name, special_type) VALUES (:name, :special_type)"_s);
  q.BindValue(u":name"_s, name);
  q.BindValue(u":special_type"_s, special_type);
  if (!q.Exec()) {
    database_->ReportErrors(q);
    return -1;
  }

  return q.lastInsertId().toInt();

}

void PlaylistBackend::RemovePlaylist(int id) {

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());

  ScopedTransaction transaction(&db);

  {
    SqlQuery q(db);
    q.prepare(u"DELETE FROM playlists WHERE ROWID=:id"_s);
    q.BindValue(u":id"_s, id);
    if (!q.Exec()) {
      database_->ReportErrors(q);
      return;
    }
  }

  {
    SqlQuery q(db);
    q.prepare(u"DELETE FROM playlist_items WHERE playlist=:id"_s);
    q.BindValue(u":id"_s, id);
    if (!q.Exec()) {
      database_->ReportErrors(q);
      return;
    }
  }

  transaction.Commit();

}

void PlaylistBackend::RenamePlaylist(const int id, const QString &new_name) {

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());
  SqlQuery q(db);
  q.prepare(u"UPDATE playlists SET name=:name WHERE ROWID=:id"_s);
  q.BindValue(u":name"_s, new_name);
  q.BindValue(u":id"_s, id);

  if (!q.Exec()) {
    database_->ReportErrors(q);
  }

}

void PlaylistBackend::FavoritePlaylist(const int id, const bool is_favorite) {

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());
  SqlQuery q(db);
  q.prepare(u"UPDATE playlists SET is_favorite=:is_favorite WHERE ROWID=:id"_s);
  q.BindValue(u":is_favorite"_s, is_favorite ? 1 : 0);
  q.BindValue(u":id"_s, id);

  if (!q.Exec()) {
    database_->ReportErrors(q);
  }

}

void PlaylistBackend::SetPlaylistOrder(const QList<int> &ids) {

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());
  ScopedTransaction transaction(&db);

  SqlQuery q(db);
  q.prepare(u"UPDATE playlists SET ui_order=-1"_s);
  if (!q.Exec()) {
    database_->ReportErrors(q);
    return;
  }

  q.prepare(u"UPDATE playlists SET ui_order=:index WHERE ROWID=:id"_s);
  for (int i = 0; i < ids.count(); ++i) {
    q.BindValue(u":index"_s, i);
    q.BindValue(u":id"_s, ids[i]);
    if (!q.Exec()) {
      database_->ReportErrors(q);
      return;
    }
  }

  transaction.Commit();

}

void PlaylistBackend::SetPlaylistUiPath(const int id, const QString &path) {

  QMutexLocker l(database_->Mutex());
  QSqlDatabase db(database_->Connect());
  SqlQuery q(db);
  q.prepare(u"UPDATE playlists SET ui_path=:path WHERE ROWID=:id"_s);

  ScopedTransaction transaction(&db);

  q.BindValue(u":path"_s, path);
  q.BindValue(u":id"_s, id);
  if (!q.Exec()) {
    database_->ReportErrors(q);
    return;
  }

  transaction.Commit();

}
