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

#include "shared_ptr.h"
#include "urlhandler.h"
#include "engine/enginebase.h"
#include "engine/enginemetadata.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "settings/behavioursettingspage.h"

class QTimer;
class Application;
class Song;
class AnalyzerContainer;
class Equalizer;
#ifdef HAVE_GSTREAMER
class GstStartup;
#endif

class PlayerInterface : public QObject {
  Q_OBJECT

 public:
  explicit PlayerInterface(QObject *parent = nullptr) : QObject(parent) {}

  virtual SharedPtr<EngineBase> engine() const = 0;
  virtual EngineBase::State GetState() const = 0;
  virtual uint GetVolume() const = 0;

  virtual PlaylistItemPtr GetCurrentItem() const = 0;
  virtual PlaylistItemPtr GetItemAt(const int pos) const = 0;

  virtual void RegisterUrlHandler(UrlHandler *handler) = 0;
  virtual void UnregisterUrlHandler(UrlHandler *handler) = 0;

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

class Player : public PlayerInterface {
  Q_OBJECT

 public:
  explicit Player(Application *app, QObject *parent = nullptr);

  static const char *kSettingsGroup;

  EngineBase::Type CreateEngine(EngineBase::Type Type);
  void Init();

  SharedPtr<EngineBase> engine() const override { return engine_; }
  EngineBase::State GetState() const override { return last_state_; }
  uint GetVolume() const override;

  PlaylistItemPtr GetCurrentItem() const override { return current_item_; }
  PlaylistItemPtr GetItemAt(const int pos) const override;

  void RegisterUrlHandler(UrlHandler *handler) override;
  void UnregisterUrlHandler(UrlHandler *handler) override;

  const UrlHandler *HandlerForUrl(const QUrl &url) const;

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

 Q_SIGNALS:
  void EngineChanged(const EngineBase::Type Type);

 private Q_SLOTS:
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

  void UrlHandlerDestroyed(QObject *object);
  void HandleLoadResult(const UrlHandler::LoadResult &result);

 private:
  void ResumePlayback();

  // Returns true if we were supposed to stop after this track.
  bool HandleStopAfter(const Playlist::AutoScroll autoscroll);

  void UnPause();

 private:
  Application *app_;
  SharedPtr<EngineBase> engine_;
#ifdef HAVE_GSTREAMER
  GstStartup *gst_startup_;
#endif
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

  QMap<QString, UrlHandler*> url_handlers_;

  QList<QUrl> loading_async_;
  uint volume_;
  uint volume_before_mute_;
  QDateTime last_pressed_previous_;

  bool continue_on_error_;
  bool greyout_;
  BehaviourSettingsPage::PreviousBehaviour menu_previousmode_;
  int seek_step_sec_;
  uint volume_increment_;

  QDateTime pause_time_;
  quint64 play_offset_nanosec_;

};

#endif  // PLAYER_H
