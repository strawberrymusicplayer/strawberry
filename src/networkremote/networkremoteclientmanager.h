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

#ifndef NETWORKREMOTECLIENTMANAGER_H
#define NETWORKREMOTECLIENTMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QList>
#include <QPointer>
#include "includes/shared_ptr.h"
#include "engine/enginebase.h"

class Player;
class PlaylistManager;
class PlaylistView;
class NetworkRemoteClient;
class QTimer;

class NetworkRemoteClientManager : public QObject{
    Q_OBJECT
public:
    explicit NetworkRemoteClientManager(const SharedPtr<Player> player, const SharedPtr<PlaylistManager> playlist_manager, QObject *parent = nullptr);
    ~NetworkRemoteClientManager();
    void AddClient(QTcpSocket *socket);
    void DisconnectAll();
    void BroadcastEngineState(EngineBase::State state);
    void BroadcastPlaylistChanged(quint32 playlist_id);
    void BroadcastPlaylistActivated(quint32 playlist_id);
    void SetPlaylistView(QPointer<PlaylistView> playlist_view);

private Q_SLOTS:
    void RemoveClient(const QSharedPointer<NetworkRemoteClient>& client);
    void Error(QAbstractSocket::SocketError socketError);
    void StateChanged();

private:
    const SharedPtr<Player> player_;
    const SharedPtr<PlaylistManager> playlist_manager_;
    QList<QSharedPointer<NetworkRemoteClient>> clients_;
    QTimer *seek_debounce_timer_ = nullptr;
    QPointer<PlaylistView> playlist_view_;
};

#endif