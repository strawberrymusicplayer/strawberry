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

#ifndef LIBREFMSCROBBLER_H
#define LIBREFMSCROBBLER_H

#include "config.h"

#include "includes/shared_ptr.h"
#include "scrobblingapi20.h"

class ScrobblerSettingsService;
class NetworkAccessManager;

class LibreFMScrobbler : public ScrobblingAPI20 {
  Q_OBJECT

 public:
  explicit LibreFMScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  static const char *kName;
  static const char *kSettingsGroup;

 private:
  static const char *kAuthUrl;
  static const char *kApiUrl;
  static const char *kCacheFile;
};

#endif  // LIBREFMSCROBBLER_H
