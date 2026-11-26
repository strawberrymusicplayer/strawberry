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

#ifndef SONOSPHERESCROBBLER_H
#define SONOSPHERESCROBBLER_H

#include "config.h"

#include "includes/shared_ptr.h"
#include "lastfmscrobbler.h"

class SonosphereScrobbler : public LastFMScrobbler {
  Q_OBJECT

 public:
  explicit SonosphereScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  static const char *kName;
  static const char *kSettingsGroup;
  static const char *kApiUrl;

  static bool HasCompiledCredentials();
};

#endif  // SONOSPHERESCROBBLER_H
