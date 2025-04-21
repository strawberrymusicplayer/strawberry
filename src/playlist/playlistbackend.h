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

#ifndef PLAYLISTBACKEND_H
#define PLAYLISTBACKEND_H

#include "config.h"

#include <QObject>
#include <QMutex>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/sqlrow.h"
#include "playlistitem.h"
#include "smartplaylists/playlistgenerator.h"

class QThread;
class Database;
class TagReaderClient;

class PlaylistBackend : public QObject {
  Q_OBJECT

 public:
  Q_INVOKABLE explicit PlaylistBackend(const SharedPtr<Database> database,
                                       const SharedPtr<TagReaderClient> tagreader_client,
                                       const SharedPtr<CollectionBackend> collection_backend,
                                       QObject *parent = nullptr);

  struct Playlist {
    Playlist() : id(-1), favorite(false), last_played(0) {}

    int id;
    QString name;
    QString ui_path;
    bool favorite;
    int last_played;
    QString special_type;
    PlaylistGenerator::Type dynamic_type;
    QString dynamic_backend;
    QByteArray dynamic_data;
  };
  using PlaylistList = QList<Playlist>;

  void Close();
  void ExitAsync();

  PlaylistList GetAllPlaylists();
  PlaylistList GetAllOpenPlaylists();
  PlaylistList GetAllFavoritePlaylists();
  PlaylistBackend::Playlist GetPlaylist(const int id);

  PlaylistItemPtrList GetPlaylistItems(const int playlist);
  SongList GetPlaylistSongs(const int playlist);

  void SetPlaylistOrder(const QList<int> &ids);
  void SetPlaylistUiPath(const int id, const QString &path);

  int CreatePlaylist(const QString &name, const QString &special_type);
  void SavePlaylistAsync(const int playlist, const PlaylistItemPtrList &items, const int last_played, PlaylistGeneratorPtr dynamic);
  void RenamePlaylist(const int id, const QString &new_name);
  void FavoritePlaylist(const int id, bool is_favorite);
  void RemovePlaylist(const int id);

 public Q_SLOTS:
  void Exit();
  void SavePlaylist(const int playlist, const PlaylistItemPtrList &items, const int last_played, PlaylistGeneratorPtr dynamic);

 Q_SIGNALS:
  void ExitFinished();

 private:
  struct NewSongFromQueryState {
    QHash<QString, SongList> cached_cues_;
    QMutex mutex_;
  };

  static QString PlaylistItemsQuery();
  Song NewSongFromQuery(const SqlRow &row, SharedPtr<NewSongFromQueryState> state);
  PlaylistItemPtr NewPlaylistItemFromQuery(const SqlRow &row, SharedPtr<NewSongFromQueryState> state);
  PlaylistItemPtr RestoreCueData(PlaylistItemPtr item, SharedPtr<NewSongFromQueryState> state);

  enum GetPlaylistsFlags {
    GetPlaylists_OpenInUi = 1,
    GetPlaylists_Favorite = 2,
    GetPlaylists_All = GetPlaylists_OpenInUi | GetPlaylists_Favorite
  };
  PlaylistList GetPlaylists(const GetPlaylistsFlags flags);

  const SharedPtr<Database> database_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<CollectionBackend> collection_backend_;
  QThread *original_thread_;
};

#endif  // PLAYLISTBACKEND_H
