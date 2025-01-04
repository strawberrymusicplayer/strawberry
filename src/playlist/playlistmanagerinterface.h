/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTMANAGERINTERFACE_H
#define PLAYLISTMANAGERINTERFACE_H

#include <QtGlobal>
#include <QObject>
#include <QItemSelectionModel>
#include <QList>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "constants/playlistsettings.h"
#include "playlist.h"
#include "smartplaylists/playlistgenerator.h"

class QModelIndex;
class CollectionBackend;
class CurrentAlbumCoverLoader;
class PlaylistBackend;
class PlaylistContainer;
class PlaylistParser;
class PlaylistSequence;

class PlaylistManagerInterface : public QObject {
  Q_OBJECT

 public:
  explicit PlaylistManagerInterface(QObject *parent);

  virtual int current_id() const = 0;
  virtual int active_id() const = 0;

  virtual QList<int> playlist_ids() const = 0;
  virtual QString playlist_name(const int id) const = 0;
  virtual Playlist *playlist(const int id) const = 0;
  virtual Playlist *current() const = 0;
  virtual Playlist *active() const = 0;

  // Returns the collection of playlists managed by this PlaylistManager.
  virtual QList<Playlist*> GetAllPlaylists() const = 0;
  // Removes all deleted songs from all playlists.
  virtual void RemoveDeletedSongs() = 0;

  virtual QItemSelection selection(const int id) const = 0;
  virtual QItemSelection current_selection() const = 0;
  virtual QItemSelection active_selection() const = 0;

  virtual QString GetPlaylistName(const int index) const = 0;

  virtual SharedPtr<CollectionBackend> collection_backend() const = 0;
  virtual SharedPtr<PlaylistBackend> playlist_backend() const = 0;
  virtual PlaylistSequence *sequence() const = 0;
  virtual PlaylistParser *parser() const = 0;
  virtual PlaylistContainer *playlist_container() const = 0;

  virtual void PlaySmartPlaylist(PlaylistGeneratorPtr generator, const bool as_new, const bool clear) = 0;

 public Q_SLOTS:
  virtual void New(const QString &name, const SongList &songs = SongList(), const QString &special_type = QString()) = 0;
  virtual void Load(const QString &filename) = 0;
  virtual void Save(const int id, const QString &playlist_name, const QString &filename, const PlaylistSettings::PathType path_type) = 0;
  virtual void Rename(const int id, const QString &new_name) = 0;
  virtual void Delete(const int id) = 0;
  virtual bool Close(const int id) = 0;
  virtual void Open(const int id) = 0;
  virtual void ChangePlaylistOrder(const QList<int> &ids) = 0;

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
  virtual void RateCurrentSong(const float rating) = 0;
  // Rate current song using 0 - 5 scale.
  virtual void RateCurrentSong2(const int rating) = 0;

 Q_SIGNALS:
  void PlaylistManagerInitialized();
  void AllPlaylistsLoaded();

  void PlaylistAdded(const int id, const QString &name, const bool favorite);
  void PlaylistDeleted(const int id);
  void PlaylistClosed(const int id);
  void PlaylistRenamed(const int id, const QString &new_name);
  void PlaylistFavorited(const int id, const bool favorite);
  void CurrentChanged(Playlist *new_playlist, const int scroll_position = 0);
  void ActiveChanged(Playlist *new_playlist);

  void Error(const QString &message);
  void SummaryTextChanged(const QString &summary);

  // Forwarded from individual playlists
  void CurrentSongChanged(const Song &song);
  void CurrentSongMetadataChanged(const Song &song);

  // Signals that one of manager's playlists has changed (new items, new ordering etc.) - the argument shows which.
  void PlaylistChanged(Playlist *playlist);
  void EditingFinished(const int playlist_id, const QModelIndex idx);
  void PlayRequested(const QModelIndex idx, const Playlist::AutoScroll autoscroll);
};

#endif  // PLAYLISTMANAGERINTERFACE_H
