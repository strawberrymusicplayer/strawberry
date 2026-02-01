/*
 * Strawberry Music Player
 * Copyright 2025-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DISCORDRPC_H
#define DISCORDRPC_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QLocalSocket>

class QTimer;

class DiscordPresence;

class DiscordRPC : public QObject {
  Q_OBJECT

 public:
  explicit DiscordRPC(const QString &application_id, QObject *parent = nullptr);
  ~DiscordRPC() override;

  void Initialize();
  void Shutdown();
  void UpdatePresence(const DiscordPresence &presence);
  void ClearPresence();

  bool IsConnected() const { return state_ == State::Connected; }

 private Q_SLOTS:
  void OnConnected();
  void OnDisconnected();
  void OnReadyRead();
  void OnError(const QLocalSocket::LocalSocketError error);
  void OnReconnectTimer();

 private:
  enum class State {
    Disconnected,
    Connecting,
    SentHandshake,
    Connected
  };

  enum class Opcode : quint32 {
    Handshake = 0,
    Frame = 1,
    Close = 2,
    Ping = 3,
    Pong = 4
  };

  void ConnectToDiscord();
  void TryNextConnection();
  void SendHandshake();
  void SendFrame(const QByteArray &data);
  void ProcessIncomingData();
  void HandleMessage(const QByteArray &data);
  void ScheduleReconnect();
  QByteArray CreateHandshakeMessage();
  QByteArray CreatePresenceMessage(const DiscordPresence &presence);

  QString application_id_;
  QLocalSocket *socket_;
  QTimer *reconnect_timer_;
  State state_;
  int nonce_;
  int reconnect_delay_;
  QByteArray read_buffer_;
  QStringList connection_paths_;
  int current_connection_index_;
  bool shutting_down_;
};

#endif  // DISCORDRPC_H
