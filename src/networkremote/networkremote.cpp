/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, Andreas Muttscheller <asfa194@gmail.com>
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QDataStream>
#include <QHostInfo>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QTcpServer>
#include <QSettings>

#include "constants/networkremotesettingsconstants.h"
#include "constants/networkremoteconstants.h"
#include "core/logging.h"
#include "core/zeroconf.h"
#include "playlist/playlistmanager.h"
#include "covermanager/currentalbumcoverloader.h"
#include "networkremote.h"
#include "incomingdataparser.h"
#include "outgoingdatacreator.h"

using namespace Qt::Literals::StringLiterals;
using namespace NetworkRemoteSettingsConstants;
using namespace NetworkRemoteConstants;
using std::make_unique;

NetworkRemote::NetworkRemote(const SharedPtr<Database> database,
                             const SharedPtr<Player> player,
                             const SharedPtr<CollectionBackend> collection_backend,
                             const SharedPtr<PlaylistManager> playlist_manager,
                             const SharedPtr<PlaylistBackend> playlist_backend,
                             const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                             const SharedPtr<AudioScrobbler> scrobbler,
                             QObject *parent)
    : QObject(parent),
      database_(database),
      player_(player),
      collection_backend_(collection_backend),
      playlist_manager_(playlist_manager),
      playlist_backend_(playlist_backend),
      current_albumcover_loader_(current_albumcover_loader),
      scrobbler_(scrobbler),
      enabled_(false),
      port_(0),
      allow_public_access_(true),
      signals_connected_(false) {

  setObjectName("NetworkRemote");

  ReloadSettings();

}

NetworkRemote::~NetworkRemote() { StopServer(); }

void NetworkRemote::ReloadSettings() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  enabled_ = s.value(kEnabled, false).toBool();
  port_ = s.value("port", kDefaultServerPort).toInt();
  allow_public_access_ = s.value(kAllowPublicAccess, false).toBool();
  s.endGroup();

  SetupServer();
  StopServer();
  StartServer();

}

void NetworkRemote::SetupServer() {

  server_ = make_unique<QTcpServer>();
  server_ipv6_ = make_unique<QTcpServer>();
  incoming_data_parser_ = make_unique<IncomingDataParser>(player_, playlist_manager_, scrobbler_);
  outgoing_data_creator_ = make_unique<OutgoingDataCreator>(database_, player_, playlist_manager_, playlist_backend_);

  outgoing_data_creator_->SetClients(&clients_);

  QObject::connect(&*current_albumcover_loader_, &CurrentAlbumCoverLoader::AlbumCoverLoaded, &*outgoing_data_creator_, &OutgoingDataCreator::CurrentSongChanged);

  QObject::connect(&*server_, &QTcpServer::newConnection, this, &NetworkRemote::AcceptConnection);
  QObject::connect(&*server_ipv6_, &QTcpServer::newConnection, this, &NetworkRemote::AcceptConnection);

  QObject::connect(&*incoming_data_parser_, &IncomingDataParser::AddToPlaylistSignal, this, &NetworkRemote::AddToPlaylistSignal);
  QObject::connect(&*incoming_data_parser_, &IncomingDataParser::SetCurrentPlaylist, this, &NetworkRemote::SetCurrentPlaylist);

}

void NetworkRemote::StartServer() {

  if (!enabled_) {
    qLog(Info) << "Network Remote deactivated";
    return;
  }

  qLog(Info) << "Starting network remote";

  server_->setProxy(QNetworkProxy::NoProxy);
  server_ipv6_->setProxy(QNetworkProxy::NoProxy);

  server_->listen(QHostAddress::Any, port_);
  server_ipv6_->listen(QHostAddress::AnyIPv6, port_);

  qLog(Info) << "Listening on port " << port_;

  if (Zeroconf::GetZeroconf()) {
    QString name = QLatin1String("Strawberry on %1").arg(QHostInfo::localHostName());
    Zeroconf::GetZeroconf()->Publish(u"local"_s, u"_strawberry._tcp"_s, name, port_);
  }

}

void NetworkRemote::StopServer() {

  if (server_->isListening()) {
    outgoing_data_creator_->DisconnectAllClients();
    server_->close();
    server_ipv6_->close();
    qDeleteAll(clients_);
    clients_.clear();
  }

}

void NetworkRemote::AcceptConnection() {

  if (!signals_connected_) {
    signals_connected_ = true;

    QObject::connect(&*incoming_data_parser_, &IncomingDataParser::SendInfo, &*outgoing_data_creator_, &OutgoingDataCreator::SendInfo);
    QObject::connect(&*incoming_data_parser_, &IncomingDataParser::SendFirstData, &*outgoing_data_creator_, &OutgoingDataCreator::SendFirstData);
    QObject::connect(&*incoming_data_parser_, &IncomingDataParser::SendAllPlaylists, &*outgoing_data_creator_, &OutgoingDataCreator::SendAllPlaylists);
    QObject::connect(&*incoming_data_parser_, &IncomingDataParser::SendAllActivePlaylists, &*outgoing_data_creator_, &OutgoingDataCreator::SendAllActivePlaylists);
    QObject::connect(&*incoming_data_parser_, &IncomingDataParser::SendPlaylistSongs, &*outgoing_data_creator_, &OutgoingDataCreator::SendPlaylistSongs);

    QObject::connect(&*playlist_manager_, &PlaylistManager::ActiveChanged, &*outgoing_data_creator_, &OutgoingDataCreator::ActiveChanged);
    QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistChanged, &*outgoing_data_creator_, &OutgoingDataCreator::PlaylistChanged);
    QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistAdded, &*outgoing_data_creator_, &OutgoingDataCreator::PlaylistAdded);
    QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistRenamed, &*outgoing_data_creator_, &OutgoingDataCreator::PlaylistRenamed);
    QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistClosed, &*outgoing_data_creator_, &OutgoingDataCreator::PlaylistClosed);
    QObject::connect(&*playlist_manager_, &PlaylistManager::PlaylistDeleted, &*outgoing_data_creator_, &OutgoingDataCreator::PlaylistDeleted);

    QObject::connect(&*player_, &Player::VolumeChanged, &*outgoing_data_creator_, &OutgoingDataCreator::VolumeChanged);
    QObject::connect(&*player_->engine(), &EngineBase::StateChanged, &*outgoing_data_creator_, &OutgoingDataCreator::StateChanged);

    QObject::connect(&*playlist_manager_->sequence(), &PlaylistSequence::RepeatModeChanged, &*outgoing_data_creator_, &OutgoingDataCreator::SendRepeatMode);
    QObject::connect(&*playlist_manager_->sequence(), &PlaylistSequence::ShuffleModeChanged, &*outgoing_data_creator_, &OutgoingDataCreator::SendShuffleMode);

    QObject::connect(&*incoming_data_parser_, &IncomingDataParser::SendCollection, &*outgoing_data_creator_, &OutgoingDataCreator::SendCollection);
    QObject::connect(&*incoming_data_parser_, &IncomingDataParser::SendListFiles, &*outgoing_data_creator_, &OutgoingDataCreator::SendListFiles);
  }

  QTcpServer *server = qobject_cast<QTcpServer*>(sender());
  QTcpSocket *client_socket = server->nextPendingConnection();

  if (!allow_public_access_ && !IpIsPrivate(client_socket->peerAddress())) {
    qLog(Warning) << "Got connection from public IP address" << client_socket->peerAddress().toString();
    client_socket->close();
    client_socket->deleteLater();
  }
  else {
    CreateRemoteClient(client_socket);
  }

}

bool NetworkRemote::IpIsPrivate(const QHostAddress &address) {

  return
    // Localhost v4
    address.isInSubnet(QHostAddress::parseSubnet(u"127.0.0.0/8"_s)) ||
    // Link Local v4
    address.isInSubnet(QHostAddress::parseSubnet(u"169.254.1.0/16"_s)) ||
    // Link Local v6
    address.isInSubnet(QHostAddress::parseSubnet(u"::1/128"_s)) ||
    address.isInSubnet(QHostAddress::parseSubnet(u"fe80::/10"_s)) ||
    // Private v4 range
    address.isInSubnet(QHostAddress::parseSubnet(u"192.168.0.0/16"_s)) ||
    address.isInSubnet(QHostAddress::parseSubnet(u"172.16.0.0/12"_s)) ||
    address.isInSubnet(QHostAddress::parseSubnet(u"10.0.0.0/8"_s)) ||
    // Private v4 range translated to v6
    address.isInSubnet(QHostAddress::parseSubnet(u"::ffff:192.168.0.0/112"_s)) ||
    address.isInSubnet(QHostAddress::parseSubnet(u"::ffff:172.16.0.0/108"_s)) ||
    address.isInSubnet(QHostAddress::parseSubnet(u"::ffff:10.0.0.0/104"_s)) ||
    // Private v6 range
    address.isInSubnet(QHostAddress::parseSubnet(u"fc00::/7"_s));

}

void NetworkRemote::CreateRemoteClient(QTcpSocket *client_socket) {

  if (client_socket) {

    NetworkRemoteClient *client = new NetworkRemoteClient(player_, collection_backend_, playlist_manager_, client_socket);
    clients_.push_back(client);

    // Update the Remote Root Files for the latest Client
    outgoing_data_creator_->SetMusicExtensions(client->files_music_extensions());
    outgoing_data_creator_->SetRemoteRootFiles(client->files_root_folder());
    incoming_data_parser_->SetRemoteRootFiles(client->files_root_folder());
    // Update OutgoingDataCreator with latest allow_downloads setting
    outgoing_data_creator_->SetAllowDownloads(client->allow_downloads());

    // Connect the signal to parse data
    QObject::connect(client, &NetworkRemoteClient::Parse, &*incoming_data_parser_, &IncomingDataParser::Parse);

    client->IncomingData();

  }

}
