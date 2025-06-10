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

#include <QObject>
#include "core/settings.h"

class NetworkRemoteSettings{
 public:
  static const char *kSettingsGroup;
  explicit NetworkRemoteSettings();
  ~NetworkRemoteSettings();
  void Load();
  void Save();
  bool UserRemote();
  bool LocalOnly();
  QString GetIpAddress();
  int GetPort();
  void SetUseRemote(bool);
  void SetLocalOnly(bool);
  void SetIpAdress ();
  void SetPort(int);

 private:
  Settings s_;
  bool enabled_;
  bool local_only_;
  int remote_port_;
  QString ip_addr_;
};

#endif
