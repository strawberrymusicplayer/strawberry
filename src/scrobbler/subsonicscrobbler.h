/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
 * Copyright 2020, Pascal Below <spezifisch@below.fr>
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

#ifndef SUBSONICSCROBBLER_H
#define SUBSONICSCROBBLER_H

#include "config.h"

#include <QDateTime>
#include <QVariant>
#include <QString>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "scrobblerservice.h"

class ScrobblerSettingsService;
class SubsonicService;

class SubsonicScrobbler : public ScrobblerService {
  Q_OBJECT

 public:
  explicit SubsonicScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, const SharedPtr<SubsonicService> service, QObject *parent = nullptr);

  void ReloadSettings() override;

  bool enabled() const override { return enabled_; }
  bool authentication_required() const override { return true; }
  bool authenticated() const override { return true; }
  bool use_authorization_header() const override { return false; }
  QByteArray authorization_header() const override { return QByteArray(); }

  void UpdateNowPlaying(const Song &song) override;
  void ClearPlaying() override;
  void Scrobble(const Song &song) override;

  void StartSubmit(const bool initial = false) override { Q_UNUSED(initial) }
  bool submitted() const override { return submitted_; }

  SharedPtr<SubsonicService> service() const;

 public Q_SLOTS:
  void WriteCache() override {}
  void Submit() override;

 private:
  const SharedPtr<SubsonicService> service_;
  bool enabled_;
  bool submitted_;
  Song song_playing_;
  QDateTime time_;
  QTimer timer_submit_;
};

#endif  // SUBSONICSCROBBLER_H
