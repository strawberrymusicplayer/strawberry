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

#ifndef PLAYLISTMANAGER_H
#define PLAYLISTMANAGER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QItemSelectionModel>
#include <QList>
#include <QMap>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "constants/playlistsettings.h"
#include "playlistmanagerinterface.h"
#include "playlist.h"
#include "smartplaylists/playlistgenerator.h"

class TaskManager;
class TagReaderClient;
class UrlHandlers;
class CollectionBackend;
class CurrentAlbumCoverLoader;
class PlaylistBackend;
class PlaylistContainer;
class PlaylistParser;
class PlaylistSequence;

class PlaylistManager : public PlaylistManagerInterface {
  Q_OBJECT

 public:
  explicit PlaylistManager(const SharedPtr<TaskManager> task_manager,
                           const SharedPtr<TagReaderClient> tagreader_client,
                           const SharedPtr<UrlHandlers> url_handlers,
                           const SharedPtr<PlaylistBackend> playlist_backend,
                           const SharedPtr<CollectionBackend> collection_backend,
                           const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                           QObject *parent = nullptr);

  ~PlaylistManager() override;

  int current_id() const override { return current_; }
  int active_id() const override { return active_; }

  QList<int> playlist_ids() const override { return playlists_.keys(); }
  QString playlist_name(const int id) const override { return playlists_[id].name; }
  Playlist *playlist(const int id) const override { return playlists_[id].p; }
  Playlist *current() const override { return playlist(current_id()); }
  Playlist *active() const override { return playlist(active_id()); }

  // Returns the collection of playlists managed by this PlaylistManager.
  QList<Playlist*> GetAllPlaylists() const override;
  // Removes all deleted songs from all playlists.
  void RemoveDeletedSongs() override;
  // Returns true if the playlist is open
  bool IsPlaylistOpen(const int id);

  QItemSelection selection(const int id) const override;
  QItemSelection current_selection() const override { return selection(current_id()); }
  QItemSelection active_selection() const override { return selection(active_id()); }

  QString GetPlaylistName(const int index) const override { return playlists_[index].name; }
  bool IsPlaylistFavorite(const int index) const { return playlists_[index].p->is_favorite(); }

  void Init(PlaylistSequence *sequence, PlaylistContainer *playlist_container);

  SharedPtr<CollectionBackend> collection_backend() const override { return collection_backend_; }
  SharedPtr<PlaylistBackend> playlist_backend() const override { return playlist_backend_; }
  PlaylistSequence *sequence() const override { return sequence_; }
  PlaylistParser *parser() const override { return parser_; }
  PlaylistContainer *playlist_container() const override { return playlist_container_; }

 public Q_SLOTS:
  void New(const QString &name, const SongList &songs = SongList(), const QString &special_type = QString()) override;
  void Load(const QString &filename) override;
  void Save(const int id, const QString &playlist_name, const QString &filename, const PlaylistSettings::PathType path_type) override;
  // Display a file dialog to let user choose a file before saving the file
  void SaveWithUI(const int id, const QString &playlist_name);
  void Rename(const int id, const QString &new_name) override;
  void Favorite(const int id, bool favorite);
  void Delete(const int id) override;
  bool Close(const int id) override;
  void Open(const int id) override;
  void ChangePlaylistOrder(const QList<int> &ids) override;

  void SetCurrentPlaylist(const int id) override;
  void SetActivePlaylist(const int id) override;
  void SetActiveToCurrent() override;

  void SelectionChanged(const QItemSelection &selection) override;

  // Makes a playlist current if it's open already, or opens it and makes it current if it is hidden.
  void SetCurrentOrOpen(const int id);

  // Convenience slots that defer to either current() or active()
  void ClearCurrent() override;
  void ShuffleCurrent() override;
  void RemoveDuplicatesCurrent() override;
  void RemoveUnavailableCurrent() override;

  void SongChangeRequestProcessed(const QUrl &url, const bool valid) override;

  void InsertUrls(const int id, const QList<QUrl> &urls, const int pos = -1, const bool play_now = false, const bool enqueue = false);
  void InsertSongs(const int id, const SongList &songs, const int pos = -1, const bool play_now = false, const bool enqueue = false);
  // Removes items with given indices from the playlist. This operation is not undoable.
  void RemoveItemsWithoutUndo(const int id, const QList<int> &indices);
  // Remove the current playing song
  void RemoveCurrentSong() const;

  void PlaySmartPlaylist(PlaylistGeneratorPtr generator, const bool as_new, const bool clear) override;

  // Rate current song using 0.0 - 1.0 scale.
  void RateCurrentSong(const float rating) override;
  // Rate current song using 0 - 5 scale.
  void RateCurrentSong2(const int rating) override;

  void SaveAllPlaylists();

  void SetActivePlaying() override;
  void SetActivePaused() override;
  void SetActiveStopped() override;

 private Q_SLOTS:
  void OneOfPlaylistsChanged();
  void UpdateSummaryText();
  void UpdateCollectionSongs(const SongList &songs);
  void ItemsLoadedForSavePlaylist(const QString &playlist_name, const SongList &songs, const QString &filename, const PlaylistSettings::PathType path_type);
  void PlaylistLoaded();

 private:
  Playlist *AddPlaylist(const int id, const QString &name, const QString &special_type, const QString &ui_path, const bool favorite);

 private:
  struct Data {
    explicit Data(Playlist *_p = nullptr, const QString &_name = QString()) : p(_p), name(_name), scroll_position(0) {}
    Playlist *p;
    QString name;
    QItemSelection selection;
    int scroll_position;
  };

  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<UrlHandlers> url_handlers_;
  const SharedPtr<PlaylistBackend> playlist_backend_;
  const SharedPtr<CollectionBackend> collection_backend_;
  const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  PlaylistSequence *sequence_;
  PlaylistParser *parser_;
  PlaylistContainer *playlist_container_;

  // key = id
  QMap<int, Data> playlists_;

  int current_;
  int active_;
  int playlists_loading_;
};

#endif  // PLAYLISTMANAGER_H
