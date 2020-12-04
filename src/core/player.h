/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QMap>
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <QSettings>

#include "urlhandler.h"
#include "engine/engine_fwd.h"
#include "engine/enginetype.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "settings/behavioursettingspage.h"

class Application;
class Song;
class AnalyzerContainer;
class Equalizer;
#ifdef HAVE_GSTREAMER
class GstStartup;
#endif

namespace Engine {
struct SimpleMetaBundle;
}  // namespace Engine

class PlayerInterface : public QObject {
  Q_OBJECT

 public:
  explicit PlayerInterface(QObject *parent = nullptr) : QObject(parent) {}

  virtual EngineBase *engine() const = 0;
  virtual Engine::State GetState() const = 0;
  virtual int GetVolume() const = 0;

  virtual PlaylistItemPtr GetCurrentItem() const = 0;
  virtual PlaylistItemPtr GetItemAt(int pos) const = 0;

  virtual void RegisterUrlHandler(UrlHandler *handler) = 0;
  virtual void UnregisterUrlHandler(UrlHandler *handler) = 0;

 public slots:
  virtual void ReloadSettings() = 0;

  // Manual track change to the specified track
  virtual void PlayAt(const int index, Engine::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const bool reshuffle, const bool force_inform = false) = 0;

  // If there's currently a song playing, pause it, otherwise play the track that was playing last, or the first one on the playlist
  virtual void PlayPause(Playlist::AutoScroll autoscroll = Playlist::AutoScroll_Always) = 0;
  virtual void RestartOrPrevious() = 0;

  // Skips this track.  Might load more of the current radio station.
  virtual void Next() = 0;
  virtual void Previous() = 0;
  virtual void PlayPlaylist(const QString &playlist_name) = 0;
  virtual void SetVolume(const int value) = 0;
  virtual void VolumeUp() = 0;
  virtual void VolumeDown() = 0;
  virtual void SeekTo(const int seconds) = 0;
  // Moves the position of the currently playing song five seconds forward.
  virtual void SeekForward() = 0;
  // Moves the position of the currently playing song five seconds backwards.
  virtual void SeekBackward() = 0;

  virtual void CurrentMetadataChanged(const Song &metadata) = 0;

  virtual void Mute() = 0;
  virtual void Pause() = 0;
  virtual void Stop(bool stop_after = false) = 0;
  virtual void Play() = 0;
  virtual void ShowOSD() = 0;

 signals:
  void Playing();
  void Paused();
  // Emitted only when playback is manually resumed
  void Resumed();
  void Stopped();
  void Error();
  void PlaylistFinished();
  void VolumeEnabled(bool);
  void VolumeChanged(int volume);
  void Error(QString message);
  void TrackSkipped(PlaylistItemPtr old_track);
  // Emitted when there's a manual change to the current's track position.
  void Seeked(qlonglong microseconds);

  // Emitted when Player has processed a request to play another song.
  // This contains the URL of the song and a flag saying whether it was able to play the song.
  void SongChangeRequestProcessed(QUrl url, bool valid);

  // The toggle parameter is true when user requests to toggle visibility for Pretty OSD
  void ForceShowOSD(Song, bool toggle);

  void Authenticated();

};

class Player : public PlayerInterface {
  Q_OBJECT

 public:
  explicit Player(Application *app, QObject *parent);
  ~Player() override;

  static const char *kSettingsGroup;

  Engine::EngineType CreateEngine(Engine::EngineType enginetype);
  void Init();

  EngineBase *engine() const override { return engine_.get(); }
  Engine::State GetState() const override{ return last_state_; }
  int GetVolume() const override;

  PlaylistItemPtr GetCurrentItem() const override { return current_item_; }
  PlaylistItemPtr GetItemAt(int pos) const override;

  void RegisterUrlHandler(UrlHandler *handler) override;
  void UnregisterUrlHandler(UrlHandler *handler) override;

  const UrlHandler *HandlerForUrl(const QUrl &url) const;

  bool PreviousWouldRestartTrack() const;

  void SetAnalyzer(AnalyzerContainer *analyzer) { analyzer_ = analyzer; }
  void SetEqualizer(Equalizer *equalizer) { equalizer_ = equalizer; }

 public slots:
  void ReloadSettings() override;

  void PlayAt(const int index, Engine::TrackChangeFlags change, const Playlist::AutoScroll autoscroll, const bool reshuffle, const bool force_inform = false) override;
  void PlayPause(Playlist::AutoScroll autoscroll = Playlist::AutoScroll_Always) override;
  void RestartOrPrevious() override;
  void Next() override;
  void Previous() override;
  void PlayPlaylist(const QString &playlist_name) override;
  void SetVolume(const int value) override;
  void VolumeUp() override { SetVolume(GetVolume() + 5); }
  void VolumeDown() override { SetVolume(GetVolume() - 5); }
  void SeekTo(const int seconds) override;
  void SeekForward() override;
  void SeekBackward() override;

  void CurrentMetadataChanged(const Song &metadata) override;

  void Mute() override;
  void Pause() override;
  void Stop(bool stop_after = false) override;
  void StopAfterCurrent();
  void Play() override;
  void ShowOSD() override;
  void TogglePrettyOSD();

  void HandleAuthentication();

 signals:
  void EngineChanged(Engine::EngineType enginetype);

 private slots:
  void EngineStateChanged(const Engine::State);
  void EngineMetadataReceived(const Engine::SimpleMetaBundle &bundle);
  void TrackAboutToEnd();
  void TrackEnded();
  // Play the next item on the playlist - disregarding radio stations like last.fm that might have more tracks.
  void NextItem(const Engine::TrackChangeFlags change, const Playlist::AutoScroll autoscroll);
  void PreviousItem(const Engine::TrackChangeFlags change);

  void NextInternal(const Engine::TrackChangeFlags, const Playlist::AutoScroll autoscroll);
  void PlayPlaylistInternal(Engine::TrackChangeFlags, const Playlist::AutoScroll autoscroll, const QString &playlist_name);

  void FatalError();
  void ValidSongRequested(const QUrl&);
  void InvalidSongRequested(const QUrl&);

  void UrlHandlerDestroyed(QObject *object);
  void HandleLoadResult(const UrlHandler::LoadResult &result);

 private:
  // Returns true if we were supposed to stop after this track.
  bool HandleStopAfter(const Playlist::AutoScroll autoscroll);

 private:
  Application *app_;
  std::unique_ptr<EngineBase> engine_;
#ifdef HAVE_GSTREAMER
  GstStartup *gst_startup_;
#endif
  AnalyzerContainer *analyzer_;
  Equalizer *equalizer_;

  QSettings settings_;

  PlaylistItemPtr current_item_;

  Engine::TrackChangeFlags stream_change_type_;
  Playlist::AutoScroll autoscroll_;
  Engine::State last_state_;
  int nb_errors_received_;

  QMap<QString, UrlHandler*> url_handlers_;

  QList<QUrl> loading_async_;
  int volume_before_mute_;
  QDateTime last_pressed_previous_;

  bool continue_on_error_;
  bool greyout_;
  BehaviourSettingsPage::PreviousBehaviour menu_previousmode_;
  int seek_step_sec_;

  bool volume_control_;

};

#endif  // PLAYER_H
