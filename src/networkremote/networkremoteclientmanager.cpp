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

#include "networkremoteclientmanager.h"
#include "networkremoteclient.h"
#include "core/application.h"
#include "core/logging.h"


NetworkRemoteClientManager::NetworkRemoteClientManager(const SharedPtr<Player> player, QObject *parent)
    : QObject(parent),
      player_(player),
      clients_() {}

NetworkRemoteClientManager::~NetworkRemoteClientManager() {}

void NetworkRemoteClientManager::AddClient(QTcpSocket *socket) {
  qLog(Debug) << "New Client connection +++++++++++++++";
  QObject::connect(socket, &QAbstractSocket::errorOccurred, this, &NetworkRemoteClientManager::Error);
  QObject::connect(socket, &QAbstractSocket::stateChanged, this, &NetworkRemoteClientManager::StateChanged);

  QSharedPointer<NetworkRemoteClient> client = QSharedPointer<NetworkRemoteClient>::create(player_);
  client->Init(socket);
  clients_.append(client);

  QWeakPointer<NetworkRemoteClient> weak_client = client;
  QObject::connect(client.data(), &NetworkRemoteClient::ClientIsLeaving, this, [this, weak_client](){
    QSharedPointer<NetworkRemoteClient> strong_client = weak_client.lock();
    if (strong_client) {
      RemoveClient(strong_client);
    }
  });
  qLog(Debug) << "Socket State is " << socket->state();
  qLog(Debug) << "There are now +++++++++++++++" << clients_.count() << "clients connected";
}

void NetworkRemoteClientManager::RemoveClient(const QSharedPointer<NetworkRemoteClient>& client) {
  clients_.removeOne(client);
  qLog(Debug) << "There are now +++++++++++++++" << clients_.count() << "clients connected";
}

void NetworkRemoteClientManager::Error(QAbstractSocket::SocketError socketError) {
  QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
  if (!socket) return;

  switch (socketError) {
  case QAbstractSocket::RemoteHostClosedError:
    qLog(Debug) << "Remote Host closed";
    break;
  case QAbstractSocket::HostNotFoundError:
    qLog(Debug) << "The host was not found.";
    break;
  case QAbstractSocket::ConnectionRefusedError:
    qLog(Debug) << "The connection was refused by the peer.";
    break;
  default:
    qLog(Warning) << "Terminal socket error:" << socket->errorString();
    // Fall through to cleanup for unhandled errors
    QSharedPointer<NetworkRemoteClient> client_to_remove;
    for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
      if (client->GetSocket() == socket) {
        client_to_remove = client;
        break;
      }
    }
    if (client_to_remove) {
      RemoveClient(client_to_remove);
    }
    break;
  }
}

void NetworkRemoteClientManager::StateChanged() {
  QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
  if (!socket) return;

  qLog(Debug) << socket->state();
  qLog(Debug) << "State Changed";

  if (socket->state() == QAbstractSocket::UnconnectedState) {
    QSharedPointer<NetworkRemoteClient> client_to_remove;
    for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
      if (client->GetSocket() == socket) {
        client_to_remove = client;
        break;
      }
    }
    if (client_to_remove) {
      RemoveClient(client_to_remove);
    }
  }
}

void NetworkRemoteClientManager::DisconnectAll() {
  qLog(Debug) << "Disconnecting all clients";
  const QList<QSharedPointer<NetworkRemoteClient>> clients_copy = clients_;
  for (const QSharedPointer<NetworkRemoteClient> &client : clients_copy) {
    QTcpSocket *socket = client->GetSocket();
    if (socket) {
      socket->disconnectFromHost();
    }
  }
  clients_.clear();
  qLog(Debug) << "All clients disconnected";
}