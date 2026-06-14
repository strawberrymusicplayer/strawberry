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

#ifndef NETWORKREMOTE_H
#define NETWORKREMOTE_H

#include <QObject>
#include <QHostAddress>

#include "networkremotetcpserver.h"
#include "networkremote/networkremotesettings.h"

class QThread;

class NetworkRemote : public QObject {
  Q_OBJECT
 public:
  explicit NetworkRemote(const SharedPtr<Player> player, QObject *parent = nullptr);
  ~NetworkRemote() override;

 public Q_SLOTS:
  void Init();
  void Update();
  void LoadSettings();
  void startTcpServer();
  void stopTcpServer();

 private:
  const SharedPtr<Player> player_;
  bool enabled_;
  bool local_only_;
  int remote_port_;
  QHostAddress ipAddr_;
  NetworkRemoteTcpServer *server_;
  NetworkRemoteSettings *settings_;
};

#endif // NETWORKREMOTE_H
