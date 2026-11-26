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

#include "config.h"
#include "apicredentials.h"

#include <QString>

#include "includes/shared_ptr.h"
#include "utilities/cryptutils.h"
#include "core/networkaccessmanager.h"

#include "scrobblersettingsservice.h"
#include "sonospherescrobbler.h"

const char *SonosphereScrobbler::kName = "Sonosphere";
const char *SonosphereScrobbler::kSettingsGroup = "Sonosphere";
const char *SonosphereScrobbler::kApiUrl = "https://sonosphere.fm/2.0/";

namespace {
constexpr char kAuthUrl[] = "https://sonosphere.fm/api/auth";
constexpr char kCacheFile[] = "sonospherescrobbler.cache";

QString CompiledApiKey() {
#ifdef SONOSPHERE_API_KEY
  return Utilities::MaybeDecryptApiCredential(QStringLiteral(SONOSPHERE_API_KEY));
#else
  return QString();
#endif
}

QString CompiledApiSecret() {
#ifdef SONOSPHERE_SHARED_SECRET
  return Utilities::MaybeDecryptApiCredential(QStringLiteral(SONOSPHERE_SHARED_SECRET));
#else
  return QString();
#endif
}
}  // namespace

bool SonosphereScrobbler::HasCompiledCredentials() {
#if defined(SONOSPHERE_API_KEY) && defined(SONOSPHERE_SHARED_SECRET)
  return true;
#else
  return false;
#endif
}

SonosphereScrobbler::SonosphereScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : LastFMScrobbler(LastFMScrobbler::Provider{QLatin1String(kName), QLatin1String(kSettingsGroup), QLatin1String(kApiUrl), QLatin1String(kAuthUrl), QLatin1String(kCacheFile), HasCompiledCredentials(), CompiledApiKey(), CompiledApiSecret()}, settings, network, parent) {}
