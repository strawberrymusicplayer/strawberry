/*
 * Strawberry Music Player
 * Copyright 2025, Leopold List <leo@zudiewiener.com>
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

#ifndef NETWORKREMOTESETTINGS_H
#define NETWORKREMOTESETTINGS_H

#include <QString>
#include "core/settings.h"

class NetworkRemoteSettings{
 public:
  static const char *kSettingsGroup;
  static constexpr int kMinUpcomingRows = 10;  // Determines how many rows of a playlist will be
  static constexpr int kMaxUpcomingRows = 100; // sent to the client
  static constexpr int kMinPort = 8888;
  static constexpr int kMaxPort = 65535;
  static constexpr int kDefaultPort = 8888;

  explicit NetworkRemoteSettings();
  ~NetworkRemoteSettings();
  void Load();
  void Save();
  bool UseRemote() const;
  int GetPort() const;
  void SetUseRemote(bool);
  void SetPort(int);
  QString GetToken() const;
  void SetToken(const QString &token);
  static QString CurrentToken();
  int GetPlaylistSize() const;
  void SetPlaylistSize(int size);
  static int CurrentPlaylistSize();

 private:
  bool enabled_;
  int remote_port_;
  Settings settings_;
  QString remote_token_;
  int playlist_size_;
};

#endif
