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

#ifndef PLAYER_H
#define PLAYER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMap>
#include <QDateTime>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/urlhandler.h"
#include "core/enginemetadata.h"
#include "engine/enginebase.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "constants/behavioursettings.h"
#include "playerinterface.h"

class QTimer;
class Song;
class TaskManager;
class UrlHandlers;
class PlaylistManager;
class AnalyzerContainer;
class Equalizer;

class Player : public PlayerInterface {
  Q_OBJECT

 public:
  explicit Player(const SharedPtr<TaskManager> task_manager, const SharedPtr<UrlHandlers> url_handlers, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent = nullptr);

  void Init();

  SharedPtr<EngineBase> engine() const override { return engine_; }
  EngineBase::State GetState() const override { return last_state_; }
  uint GetVolume() const override;

  PlaylistItemPtr GetCurrentItem() const override { return current_item_; }
  PlaylistItemPtr GetItemAt(const int pos) const override;

  bool PreviousWouldRestartTrack() const;

  void SetAnalyzer(AnalyzerContainer *analyzer) { analyzer_ = analyzer; }
  void SetEqualizer(SharedPtr<Equalizer> equalizer) { equalizer_ = equalizer; }

 public Q_SLOTS:
  void ReloadSettings() override;

  void LoadVolume() override;
  void SaveVolume() override;
  void SavePlaybackStatus() override;
  void PlaylistsLoaded() override;

  void PlayAt(const int index, const bool pause, const quint64 offset_nanosec, EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const bool reshuffle, const bool force_inform = false) override;
  void PlayPause(const quint64 offset_nanosec = 0, const Playlist::AutoScroll autoscroll = Playlist::AutoScroll::Always) override;
  void PlayPauseHelper() override { PlayPause(play_offset_nanosec_); }
  void RestartOrPrevious() override;
  void Next() override;
  void Previous() override;
  void PlayPlaylist(const QString &playlist_name) override;
  void SetVolumeFromSlider(const int value) override;
  void SetVolumeFromEngine(const uint volume) override;
  void SetVolume(const uint volume) override;
  void VolumeUp() override;
  void VolumeDown() override;
  void SeekTo(const quint64 seconds) override;
  void SeekForward() override;
  void SeekBackward() override;

  void CurrentMetadataChanged(const Song &metadata) override;

  void Mute() override;
  void Pause() override;
  void Stop(const bool stop_after = false) override;
  void StopAfterCurrent();
  void Play(const quint64 offset_nanosec = 0) override;
  void PlayWithPause(const quint64 offset_nanosec) override;
  void PlayHelper() override { Play(); }
  void ShowOSD() override;
  void TogglePrettyOSD();

  void HandleAuthentication();

 private Q_SLOTS:
  void UrlHandlerRegistered(UrlHandler *url_handler) const;

  void EngineStateChanged(const EngineBase::State);
  void EngineMetadataReceived(const EngineMetadata &engine_metadata);
  void TrackAboutToEnd();
  void TrackEnded();
  // Play the next item on the playlist - disregarding radio stations like last.fm that might have more tracks.
  void NextItem(const EngineBase::TrackChangeFlags change, const Playlist::AutoScroll autoscroll);
  void PreviousItem(const EngineBase::TrackChangeFlags change);

  void NextInternal(const EngineBase::TrackChangeFlags, const Playlist::AutoScroll autoscroll);
  void PlayPlaylistInternal(const EngineBase::TrackChangeFlags, const Playlist::AutoScroll autoscroll, const QString &playlist_name);

  void FatalError();
  void ValidSongRequested(const QUrl&);
  void InvalidSongRequested(const QUrl&);

  void HandleLoadResult(const UrlHandler::LoadResult &result);

 private:
  void ResumePlayback();

  // Returns true if we were supposed to stop after this track.
  bool HandleStopAfter(const Playlist::AutoScroll autoscroll);

  void UnPause();

 private:
  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<UrlHandlers> url_handlers_;
  const SharedPtr<PlaylistManager> playlist_manager_;
  SharedPtr<EngineBase> engine_;
  AnalyzerContainer *analyzer_;
  SharedPtr<Equalizer> equalizer_;
  QTimer *timer_save_volume_;

  bool playlists_loaded_;
  bool play_requested_;

  PlaylistItemPtr current_item_;

  bool pause_;
  EngineBase::TrackChangeFlags stream_change_type_;
  Playlist::AutoScroll autoscroll_;
  EngineBase::State last_state_;
  int nb_errors_received_;

  QList<QUrl> loading_async_;
  uint volume_;
  uint volume_before_mute_;
  QDateTime last_pressed_previous_;

  bool continue_on_error_;
  bool greyout_;
  BehaviourSettings::PreviousBehaviour menu_previousmode_;
  int seek_step_sec_;
  uint volume_increment_;

  QDateTime pause_time_;
  quint64 play_offset_nanosec_;
};

#endif  // PLAYER_H
