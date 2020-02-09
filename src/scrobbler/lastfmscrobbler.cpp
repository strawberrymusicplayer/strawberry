/*
 * Strawberry Music Player
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

#include "config.h"

#include <QObject>

#include "core/application.h"
#include "core/network.h"

#include "scrobblercache.h"
#include "scrobblingapi20.h"
#include "lastfmscrobbler.h"

const char *LastFMScrobbler::kName = "Last.fm";
const char *LastFMScrobbler::kSettingsGroup = "LastFM";
const char *LastFMScrobbler::kAuthUrl = "https://www.last.fm/api/auth/";
const char *LastFMScrobbler::kApiUrl = "https://ws.audioscrobbler.com/2.0/";
const char *LastFMScrobbler::kCacheFile = "lastfmscrobbler.cache";

LastFMScrobbler::LastFMScrobbler(Application *app, QObject *parent) : ScrobblingAPI20(kName, kSettingsGroup, kAuthUrl, kApiUrl, true, app, parent),
  auth_url_(kAuthUrl),
  api_url_(kApiUrl),
  app_(app),
  network_(new NetworkAccessManager(this)),
  cache_(new ScrobblerCache(kCacheFile, this)),
  enabled_(false),
  subscriber_(false),
  submitted_(false) {

  ReloadSettings();
  LoadSession();

}

LastFMScrobbler::~LastFMScrobbler() {}
