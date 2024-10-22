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

#ifndef PLAYERINTERFACE_H
#define PLAYERINTERFACE_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "engine/enginebase.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"

class QTimer;
class Song;

class PlayerInterface : public QObject {
  Q_OBJECT

 public:
  explicit PlayerInterface(QObject *parent = nullptr);

  virtual SharedPtr<EngineBase> engine() const = 0;
  virtual EngineBase::State GetState() const = 0;
  virtual uint GetVolume() const = 0;

  virtual PlaylistItemPtr GetCurrentItem() const = 0;
  virtual PlaylistItemPtr GetItemAt(const int pos) const = 0;

 public Q_SLOTS:
  virtual void ReloadSettings() = 0;
  virtual void LoadVolume() = 0;
  virtual void SaveVolume() = 0;
  virtual void SavePlaybackStatus() = 0;

  virtual void PlaylistsLoaded() = 0;

  // Manual track change to the specified track
  virtual void PlayAt(const int index, const bool pause, const quint64 offset_nanosec, EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const bool reshuffle, const bool force_inform = false) = 0;

  // If there's currently a song playing, pause it, otherwise play the track that was playing last, or the first one on the playlist
  virtual void PlayPause(const quint64 offset_nanosec = 0, const Playlist::AutoScroll autoscroll = Playlist::AutoScroll::Always) = 0;
  virtual void PlayPauseHelper() = 0;
  virtual void RestartOrPrevious() = 0;

  // Skips this track.  Might load more of the current radio station.
  virtual void Next() = 0;
  virtual void Previous() = 0;
  virtual void PlayPlaylist(const QString &playlist_name) = 0;
  virtual void SetVolumeFromEngine(const uint volume) = 0;
  virtual void SetVolumeFromSlider(const int value) = 0;
  virtual void SetVolume(const uint volume) = 0;
  virtual void VolumeUp() = 0;
  virtual void VolumeDown() = 0;
  virtual void SeekTo(const quint64 seconds) = 0;
  // Moves the position of the currently playing song five seconds forward.
  virtual void SeekForward() = 0;
  // Moves the position of the currently playing song five seconds backwards.
  virtual void SeekBackward() = 0;

  virtual void CurrentMetadataChanged(const Song &metadata) = 0;

  virtual void Mute() = 0;
  virtual void Pause() = 0;
  virtual void Stop(const bool stop_after = false) = 0;
  virtual void Play(const quint64 offset_nanosec = 0) = 0;
  virtual void PlayWithPause(const quint64 offset_nanosec) = 0;
  virtual void PlayHelper() = 0;
  virtual void ShowOSD() = 0;

 Q_SIGNALS:
  void Playing();
  void Paused();
  // Emitted only when playback is manually resumed
  void Resumed();
  void Stopped();
  void Error(const QString &message = QString());
  void PlaylistFinished();
  void VolumeEnabled(const bool volume_enabled);
  void VolumeChanged(const uint volume);
  void TrackSkipped(PlaylistItemPtr old_track);
  // Emitted when there's a manual change to the current's track position.
  void Seeked(const qint64 microseconds);

  // Emitted when Player has processed a request to play another song.
  // This contains the URL of the song and a flag saying whether it was able to play the song.
  void SongChangeRequestProcessed(const QUrl &url, const bool valid);

  // The toggle parameter is true when user requests to toggle visibility for Pretty OSD
  void ForceShowOSD(const Song &song, const bool toggle);

  void Authenticated();
};

#endif  // PLAYERINTERFACE_H
