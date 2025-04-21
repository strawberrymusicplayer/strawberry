/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"

#include "scrobblersettingsservice.h"
#include "scrobblingapi20.h"
#include "librefmscrobbler.h"

const char *LibreFMScrobbler::kName = "Libre.fm";
const char *LibreFMScrobbler::kSettingsGroup = "LibreFM";
const char *LibreFMScrobbler::kAuthUrl = "https://www.libre.fm/api/auth/";
const char *LibreFMScrobbler::kApiUrl = "https://libre.fm/2.0/";
const char *LibreFMScrobbler::kCacheFile = "librefmscrobbler.cache";

LibreFMScrobbler::LibreFMScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : ScrobblingAPI20(QLatin1String(kName), QLatin1String(kSettingsGroup), QLatin1String(kAuthUrl), QLatin1String(kApiUrl), false, QLatin1String(kCacheFile), settings, network, parent) {}
