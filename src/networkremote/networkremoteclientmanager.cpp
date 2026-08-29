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

#include <QTimer>
#include "networkremoteclientmanager.h"
#include "networkremoteclient.h"
#include "networkremotehelper.h"
#include "networkremotesettings.h"
#include "core/application.h"
#include "core/logging.h"
#include "core/player.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlist.h"
#include "core/song.h"

using namespace nwr_types;


NetworkRemoteClientManager::NetworkRemoteClientManager(const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent)
    : QObject(parent),
      player_(player),
      playlist_manager_(playlist_manager),
      clients_() {
  QObject::connect(&*player_, &Player::Playing, this, [this]() { BroadcastEngineState(EngineBase::State::Playing); });
  QObject::connect(&*player_, &Player::Paused, this, [this]() { BroadcastEngineState(EngineBase::State::Paused); });
  QObject::connect(&*player_, &Player::Stopped, this, [this]() { BroadcastEngineState(EngineBase::State::Idle); });
  QObject::connect(&*playlist_manager_, &PlaylistManager::CurrentSongChanged, this, [this](const Song &song) {
    Q_UNUSED(song);
    qLog(Debug) << "Current song changed - notifying clients";
    BroadcastEngineState(player_->GetState());

    const int active_id = playlist_manager_->active_id();
    Playlist *active_playlist = playlist_manager_->playlist(active_id);
    if (!active_playlist) {
      last_known_active_playlist_id_ = -1;
      last_known_current_row_ = -1;
      return;
    }

    const int new_row = active_playlist->current_row();
    const bool is_simple_advance =
      (active_id == last_known_active_playlist_id_) &&
      (last_known_current_row_ >= 0) &&
      (new_row == last_known_current_row_ + 1);

    if (is_simple_advance) {
      BroadcastPlaylistAdvanced(static_cast<quint32>(active_id), static_cast<quint32>(new_row));
    }
    else {
      BroadcastPlaylistSongsFull(static_cast<quint32>(active_id));
    }

    last_known_active_playlist_id_ = active_id;
    last_known_current_row_ = new_row;
  });

  // A playlist becoming the active (audio-producing) one - e.g. from a remote RequestPlaySong, or from the desktop UI itself. Clients also get a fresh window for the newly-active playlist, since they may not have an up-to-date one (or any at all) for it.
  QObject::connect(&*playlist_manager_, &PlaylistManager::ActiveChanged, this, [this](Playlist *pl) {
    if (pl) {
      BroadcastPlaylistActivated(static_cast<quint32>(pl->id()));
      BroadcastPlaylistSongsFull(static_cast<quint32>(pl->id()));
    }
  });

  // Any open playlist's song list changed (songs added/removed/reordered).
  // Only the active playlist's window is proactively resent - a non-active open playlist stays request-driven, as before.
  QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistChanged, this, [this](Playlist *pl) {
    if (!pl) return;
    BroadcastPlaylistChanged(static_cast<quint32>(pl->id()));
    if (pl->id() == playlist_manager_->active_id()) {
      BroadcastPlaylistSongsFull(static_cast<quint32>(pl->id()));
    }
  });

  // Seeking emits Seeked on every mouse-move while dragging the slider; coalesce the burst into a single broadcast once the drag settles.
  seek_debounce_timer_ = new QTimer(this);
  seek_debounce_timer_->setSingleShot(true);
  seek_debounce_timer_->setInterval(300);
  QObject::connect(seek_debounce_timer_, &QTimer::timeout, this, [this]() {
    BroadcastEngineState(player_->GetState());
  });

  QObject::connect(&*player_, &Player::Seeked, this, [this](const qint64 microseconds) {
    Q_UNUSED(microseconds);
    seek_debounce_timer_->start();
  });
}

NetworkRemoteClientManager::~NetworkRemoteClientManager() {}

void NetworkRemoteClientManager::AddClient(QTcpSocket *socket) {
  qLog(Debug) << "New Client connection +++++++++++++++";
  QObject::connect(socket, &QAbstractSocket::errorOccurred, this, &NetworkRemoteClientManager::Error);
  QObject::connect(socket, &QAbstractSocket::stateChanged, this, &NetworkRemoteClientManager::StateChanged);
  QSharedPointer<NetworkRemoteClient> client = QSharedPointer<NetworkRemoteClient>::create(player_, playlist_manager_);
  client->SetPlaylistView(playlist_view_);
  client->Init(socket);
  clients_.append(client);

  QWeakPointer<NetworkRemoteClient> weak_client = client;
  // Queued: ClientIsLeaving is emitted from within the client's own message processing, so removal must not run while that code is still on the stack.
  QObject::connect(client.data(), &NetworkRemoteClient::ClientIsLeaving, this, [this, weak_client]() {
        QSharedPointer<NetworkRemoteClient> strong_client = weak_client.lock();
        if (strong_client) {
            RemoveClient(strong_client);
        } }, Qt::QueuedConnection);
  qLog(Debug) << "Socket State is " << socket->state();
  qLog(Debug) << "There are now +++++++++++++++" << clients_.count() << "clients connected";
}

void NetworkRemoteClientManager::RemoveClient(const QSharedPointer<NetworkRemoteClient> &client) {
  clients_.removeOne(client);

  QTcpSocket *socket = client->GetSocket();
  if (socket) {
    qLog(Debug) << "Closing socket" << socket->socketDescriptor() << "for removed client";
    socket->disconnectFromHost();
    socket->deleteLater();
  }

  QSharedPointer<NetworkRemoteClient> deferred = client;
  QMetaObject::invokeMethod(this, [deferred]() {}, Qt::QueuedConnection);
  qLog(Debug) << "There are now +++++++++++++++" << clients_.count() << "clients connected";
}

void NetworkRemoteClientManager::Error(QAbstractSocket::SocketError socketError) {
  QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
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

void NetworkRemoteClientManager::SetPlaylistView(QPointer<PlaylistView> playlist_view) {
  playlist_view_ = playlist_view;
  for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
    client->SetPlaylistView(playlist_view_);
  }
}

void NetworkRemoteClientManager::BroadcastEngineState(EngineBase::State state) {
  qLog(Debug) << "Broadcasting engine state to" << clients_.count() << "clients";
  for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
    client->SendEngineState(state);
  }
}

void NetworkRemoteClientManager::BroadcastPlaylistChanged(quint32 playlist_id) {
  qLog(Debug) << "Broadcasting playlist" << playlist_id << "changed to" << clients_.count() << "clients";
  for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
    client->SendPlaylistChanged(playlist_id);
  }
}

void NetworkRemoteClientManager::BroadcastPlaylistActivated(quint32 playlist_id) {
  qLog(Debug) << "Broadcasting playlist" << playlist_id << "activated to" << clients_.count() << "clients";
  for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
    client->SendPlaylistActivated(playlist_id);
  }
}

void NetworkRemoteClientManager::BroadcastAuthStatus(bool auth_enabled) {
  qLog(Debug) << "Broadcasting auth status" << auth_enabled << "to" << clients_.count() << "clients";
  for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
    client->SendAuthStatusChanged(auth_enabled);
  }
}

void NetworkRemoteClientManager::BroadcastPlaylistSongsFull(quint32 playlist_id) {
  const quint32 upcoming_count = static_cast<quint32>(NetworkRemoteSettings::CurrentPlaylistSize());
  qLog(Debug) << "Broadcasting full playlist songs for playlist" << playlist_id << "to" << clients_.count() << "clients";
  for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
    client->SendPlaylistSongs(playlist_id, upcoming_count);
  }
}

void NetworkRemoteClientManager::BroadcastPlaylistAdvanced(quint32 playlist_id, quint32 new_current_row) {
  qLog(Debug) << "Broadcasting playlist advanced for playlist" << playlist_id << "row" << new_current_row << "to" << clients_.count() << "clients";
  for (const QSharedPointer<NetworkRemoteClient> &client : std::as_const(clients_)) {
    client->SendPlaylistAdvanced(playlist_id, new_current_row);
  }
}

void NetworkRemoteClientManager::DisconnectAll() {
  qLog(Debug) << "Disconnecting all clients";
  const QList<QSharedPointer<NetworkRemoteClient>> clients_copy = clients_;
  for (const QSharedPointer<NetworkRemoteClient> &client : clients_copy) {
    QTcpSocket *socket = client->GetSocket();
    qLog(Debug) << "Sending shutdown notice to socket" << (socket ? socket->socketDescriptor() : -1);
    client->SendDisconnect(ReasonDisconnect::REASON_DISCONNECT_SERVER_SHUTDOWN);
    if (socket) {
      socket->flush();
      socket->disconnectFromHost();
    }
  }
  clients_.clear();
  qLog(Debug) << "All clients disconnected";
}