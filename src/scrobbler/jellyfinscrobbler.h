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

#include <QDateTime>
#include <QVariant>
#include <QString>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "scrobblerservice.h"

class ScrobblerSettingsService;
class JellyfinService;

class JellyfinScrobbler : public ScrobblerService {
  Q_OBJECT

 public:
  explicit JellyfinScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, const SharedPtr<JellyfinService> service, QObject *parent = nullptr);

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
  void ScheduleSendScrobbles();

  bool submitted() const override { return submitted_; }

  SharedPtr<JellyfinService> service() const;

 public Q_SLOTS:
  void WriteCache() override {}

 private Q_SLOTS:
  void ScrobbleError(const QString &error);

 private:
  void SendScrobbles();
  void SendPlaybackProgress();

  const SharedPtr<JellyfinService> service_;
  bool enabled_;
  bool submitted_;
  bool scrobble_pending_;
  Song song_playing_;
  QDateTime time_;
  Song scrobble_song_;  // The track and start time of a delayed report, the playing track can change before it is sent.
  QDateTime scrobble_time_;
  QTimer timer_send_scrobbles_;
  QTimer timer_playback_progress_;
};

#endif  // JELLYFINSCROBBLER_H