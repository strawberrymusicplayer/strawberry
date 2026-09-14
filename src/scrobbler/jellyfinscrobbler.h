/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#ifndef JELLYFINSCROBBLER_H
#define JELLYFINSCROBBLER_H

#include "config.h"

#include <QString>
#include <QTimer>
#include <QElapsedTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "scrobblerservice.h"

class ScrobblerSettingsService;
class JellyfinService;
class Player;
class PlaylistManager;

class JellyfinScrobbler : public ScrobblerService {
  Q_OBJECT

 public:
  explicit JellyfinScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, const SharedPtr<JellyfinService> service, const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent = nullptr);

  void ReloadSettings() override;

  bool enabled() const override { return enabled_; }
  bool authentication_required() const override { return true; }
  bool authenticated() const override { return true; }
  bool use_authorization_header() const override { return false; }
  QByteArray authorization_header() const override { return QByteArray(); }

  void UpdateNowPlaying(const Song &song) override;
  void ClearPlaying() override;
  void Scrobble(const Song &song) override;

  void Start(const bool initial = false) override;
  void Stop() override;

  SharedPtr<JellyfinService> service() const;

 public Q_SLOTS:
  void WriteCache() override {}

 private Q_SLOTS:
  void ScrobbleError(const QString &error);
  void Paused();
  void Resumed();
  void Playing();
  void Seeked(const qint64 microseconds);
  void PlaybackProgressTimeout();
  void ResyncPosition();
  void CurrentSongChanged(const Song &song);
  void PlayerStopped();
  void PlaybackReportsCleared();

 private:
  bool IsPlayingSong() const;
  bool EngineHasSong() const;
  bool CheckPlaying();
  bool ReportingActive() const;
  bool SongChanged(const Song &song);
  void StartResyncPosition();
  void StartPlayback();
  void StopReporting();
  void UpdatePosition();
  qint64 PositionNanosec() const;
  void SendPlaybackProgress();
  void ReportStopped();

  const SharedPtr<JellyfinService> service_;
  const SharedPtr<Player> player_;
  bool enabled_;
  Song song_playing_;
  QString current_song_id_;  // The Jellyfin song that is current in the playlist, empty for other songs.
  bool new_play_;  // The current song started while reporting was active, UpdateNowPlaying() makes its playback start pending.
  bool start_pending_;  // Playback start of song_playing_ is reported when the engine plays it.
  bool playback_reported_;  // Playback start of song_playing_ was reported, and stop was not reported yet.
  bool paused_;
  qint64 position_nanosec_;  // Playback position of the playing track when position_timer_ was started.
  QElapsedTimer position_timer_;
  QTimer timer_playback_progress_;
  QTimer timer_resync_position_;
  int resync_position_attempts_;
};

#endif  // JELLYFINSCROBBLER_H
