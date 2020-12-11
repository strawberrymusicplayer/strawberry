/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QFuture>
#include <QList>
#include <QMap>
#include <QString>
#include <QUrl>

#include "core/song.h"
#include "playlist.h"
#include "smartplaylists/playlistgenerator.h"

class QModelIndex;

class Application;
class CollectionBackend;
class PlaylistBackend;
class PlaylistContainer;
class PlaylistParser;
class PlaylistSequence;

class PlaylistManagerInterface : public QObject {
  Q_OBJECT

 public:
  explicit PlaylistManagerInterface(Application *app, QObject *parent) : QObject(parent) { Q_UNUSED(app); }

  virtual int current_id() const = 0;
  virtual int active_id() const = 0;

  virtual Playlist *playlist(const int id) const = 0;
  virtual Playlist *current() const = 0;
  virtual Playlist *active() const = 0;

  // Returns the collection of playlists managed by this PlaylistManager.
  virtual QList<Playlist*> GetAllPlaylists() const = 0;
  // Grays out and reloads all deleted songs in all playlists.
  virtual void InvalidateDeletedSongs() = 0;
  // Removes all deleted songs from all playlists.
  virtual void RemoveDeletedSongs() = 0;

  virtual QItemSelection selection(const int id) const = 0;
  virtual QItemSelection current_selection() const = 0;
  virtual QItemSelection active_selection() const = 0;

  virtual QString GetPlaylistName(const int index) const = 0;

  virtual CollectionBackend *collection_backend() const = 0;
  virtual PlaylistBackend *playlist_backend() const = 0;
  virtual PlaylistSequence *sequence() const = 0;
  virtual PlaylistParser *parser() const = 0;
  virtual PlaylistContainer *playlist_container() const = 0;

  virtual void PlaySmartPlaylist(PlaylistGeneratorPtr generator, const bool as_new, const bool clear) = 0;

 public slots:
  virtual void New(const QString &name, const SongList& songs = SongList(), const QString &special_type = QString()) = 0;
  virtual void Load(const QString &filename) = 0;
  virtual void Save(const int id, const QString &filename, const Playlist::Path path_type) = 0;
  virtual void Rename(const int id, const QString &new_name) = 0;
  virtual void Delete(const int id) = 0;
  virtual bool Close(const int id) = 0;
  virtual void Open(const int id) = 0;
  virtual void ChangePlaylistOrder(const QList<int>& ids) = 0;

  virtual void SongChangeRequestProcessed(const QUrl &url, const bool valid) = 0;

  virtual void SetCurrentPlaylist(const int id) = 0;
  virtual void SetActivePlaylist(const int id) = 0;
  virtual void SetActiveToCurrent() = 0;

  virtual void SelectionChanged(const QItemSelection &selection) = 0;

  // Convenience slots that defer to either current() or active()
  virtual void ClearCurrent() = 0;
  virtual void ShuffleCurrent() = 0;
  virtual void RemoveDuplicatesCurrent() = 0;
  virtual void RemoveUnavailableCurrent() = 0;
  virtual void SetActivePlaying() = 0;
  virtual void SetActivePaused() = 0;
  virtual void SetActiveStopped() = 0;

  // Rate current song using 0.0 - 1.0 scale.
  virtual void RateCurrentSong(const double rating) = 0;
  // Rate current song using 0 - 5 scale.
  virtual void RateCurrentSong(const int rating) = 0;

 signals:
  void PlaylistManagerInitialized();
  void AllPlaylistsLoaded();

  void PlaylistAdded(const int id, QString name, const bool favorite);
  void PlaylistDeleted(const int id);
  void PlaylistClosed(const int id);
  void PlaylistRenamed(const int id, QString new_name);
  void PlaylistFavorited(const int id, const bool favorite);
  void CurrentChanged(Playlist *new_playlist, const int scroll_position = 0);
  void ActiveChanged(Playlist *new_playlist);

  void Error(QString message);
  void SummaryTextChanged(QString summary);

  // Forwarded from individual playlists
  void CurrentSongChanged(Song song);
  void SongMetadataChanged(Song song);

  // Signals that one of manager's playlists has changed (new items, new ordering etc.) - the argument shows which.
  void PlaylistChanged(Playlist *playlist);
  void EditingFinished(QModelIndex idx);
  void PlayRequested(QModelIndex idx, Playlist::AutoScroll autoscroll);
};

class PlaylistManager : public PlaylistManagerInterface {
  Q_OBJECT

 public:
  explicit PlaylistManager(Application *app, QObject *parent = nullptr);
  ~PlaylistManager() override;

  int current_id() const override { return current_; }
  int active_id() const override { return active_; }

  Playlist *playlist(const int id) const override { return playlists_[id].p; }
  Playlist *current() const override { return playlist(current_id()); }
  Playlist *active() const override { return playlist(active_id()); }

  // Returns the collection of playlists managed by this PlaylistManager.
  QList<Playlist*> GetAllPlaylists() const override;
  // Grays out and reloads all deleted songs in all playlists.
  void InvalidateDeletedSongs() override;
  // Removes all deleted songs from all playlists.
  void RemoveDeletedSongs() override;
  // Returns true if the playlist is open
  bool IsPlaylistOpen(const int id);

  // Returns a pretty automatic name for playlist created from the given list of songs.
  static QString GetNameForNewPlaylist(const SongList& songs);

  QItemSelection selection(const int id) const override;
  QItemSelection current_selection() const override { return selection(current_id()); }
  QItemSelection active_selection() const override { return selection(active_id()); }

  QString GetPlaylistName(const int index) const override { return playlists_[index].name; }
  bool IsPlaylistFavorite(const int index) const { return playlists_[index].p->is_favorite(); }

  void Init(CollectionBackend *collection_backend, PlaylistBackend *playlist_backend, PlaylistSequence *sequence, PlaylistContainer *playlist_container);

  CollectionBackend *collection_backend() const override { return collection_backend_; }
  PlaylistBackend *playlist_backend() const override { return playlist_backend_; }
  PlaylistSequence *sequence() const override { return sequence_; }
  PlaylistParser *parser() const override { return parser_; }
  PlaylistContainer *playlist_container() const override { return playlist_container_; }

 public slots:
  void New(const QString &name, const SongList &songs = SongList(), const QString &special_type = QString()) override;
  void Load(const QString &filename) override;
  void Save(const int id, const QString &filename, const Playlist::Path path_type) override;
  // Display a file dialog to let user choose a file before saving the file
  void SaveWithUI(const int id, const QString &playlist_name);
  void Rename(const int id, const QString &new_name) override;
  void Favorite(const int id, bool favorite);
  void Delete(const int id) override;
  bool Close(const int id) override;
  void Open(const int id) override;
  void ChangePlaylistOrder(const QList<int>& ids) override;

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

  void SongChangeRequestProcessed(const QUrl& url, const bool valid) override;

  void InsertUrls(const int id, const QList<QUrl>& urls, const int pos = -1, const bool play_now = false, const bool enqueue = false);
  void InsertSongs(const int id, const SongList& songs, const int pos = -1, const bool play_now = false, const bool enqueue = false);
  // Removes items with given indices from the playlist. This operation is not undoable.
  void RemoveItemsWithoutUndo(const int id, const QList<int>& indices);
  // Remove the current playing song
  void RemoveCurrentSong();

  void PlaySmartPlaylist(PlaylistGeneratorPtr generator, const bool as_new, const bool clear) override;

  // Rate current song using 0.0 - 1.0 scale.
  void RateCurrentSong(const double rating) override;
  // Rate current song using 0 - 5 scale.
  void RateCurrentSong(const int rating) override;

 private slots:
  void SetActivePlaying() override;
  void SetActivePaused() override;
  void SetActiveStopped() override;

  void OneOfPlaylistsChanged();
  void UpdateSummaryText();
  void SongsDiscovered(const SongList& songs);
  void ItemsLoadedForSavePlaylist(QFuture<SongList> future, const QString& filename, const Playlist::Path path_type);
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

  Application *app_;
  PlaylistBackend *playlist_backend_;
  CollectionBackend *collection_backend_;
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
