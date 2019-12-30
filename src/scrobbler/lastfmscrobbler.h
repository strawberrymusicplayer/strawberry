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

#ifndef LASTFMSCROBBLER_H
#define LASTFMSCROBBLER_H

#include "config.h"


#include <QtGlobal>
#include <QObject>
#include <QString>

#include "core/song.h"
#include "scrobblerservice.h"
#include "scrobblingapi20.h"

class Application;
class NetworkAccessManager;
class ScrobblerCache;

class LastFMScrobbler : public ScrobblingAPI20 {
  Q_OBJECT

 public:
  explicit LastFMScrobbler(Application *app, QObject *parent = nullptr);
  ~LastFMScrobbler();

  static const char *kName;
  static const char *kSettingsGroup;

  NetworkAccessManager *network() { return network_; }
  ScrobblerCache *cache() { return cache_; }

 private:
  static const char *kAuthUrl;
  static const char *kApiUrl;
  static const char *kCacheFile;

  QString settings_group_;
  QString auth_url_;
  QString api_url_;
  QString api_key_;
  QString secret_;
  Application *app_;
  NetworkAccessManager *network_;
  ScrobblerCache *cache_;
  bool enabled_;
  bool subscriber_;
  QString username_;
  QString session_key_;
  bool submitted_;
  Song song_playing_;
  quint64 timestamp_;

};

#endif  // LASTFMSCROBBLER_H
