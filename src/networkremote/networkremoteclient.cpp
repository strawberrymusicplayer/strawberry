/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, Andreas Muttscheller <asfa194@gmail.com>
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

#include <QDataStream>
#include <QTcpSocket>
#include <QProtobufSerializer>
#include <QSettings>

#include "constants/networkremotesettingsconstants.h"
#include "core/logging.h"
#include "networkremote.h"
#include "networkremoteclient.h"
#include "networkremotemessages.qpb.h"

using namespace Qt::Literals::StringLiterals;
using namespace NetworkRemoteSettingsConstants;

NetworkRemoteClient::NetworkRemoteClient(const SharedPtr<Player> player,
                                         const SharedPtr<CollectionBackend> collection_backend,
                                         const SharedPtr<PlaylistManager> playlist_manager,
                                         QTcpSocket *socket,
                                         QObject *parent)
    : QObject(parent),
      player_(player),
      socket_(socket),
      downloader_(false),
      reading_protobuf_(false),
      expected_length_(0),
      song_sender_(new SongSender(player, collection_backend, playlist_manager, this)) {

  QObject::connect(socket, &QTcpSocket::readyRead, this, &NetworkRemoteClient::ReadyRead);
  QObject::connect(socket, &QTcpSocket::channelReadyRead, this, &NetworkRemoteClient::ReadyRead);

  QSettings s;
  s.beginGroup(kSettingsGroup);
  use_auth_code_ = s.value(kUseAuthCode, false).toBool();
  auth_code_ = s.value(kAuthCode, 0).toInt();
  files_root_folder_ = s.value(kFilesRootFolder, ""_L1).toString();
  s.endGroup();

  authenticated_ = !use_auth_code_;

}

NetworkRemoteClient::~NetworkRemoteClient() {

  socket_->close();
  if (socket_->state() == QAbstractSocket::ConnectedState) {
    socket_->waitForDisconnected(2000);
  }

  song_sender_->deleteLater();
  socket_->deleteLater();

}

void NetworkRemoteClient::setDownloader(const bool downloader) { downloader_ = downloader; }

void NetworkRemoteClient::ReadyRead() {

  IncomingData();

}

void NetworkRemoteClient::IncomingData() {

  while (socket_->bytesAvailable()) {
    if (!reading_protobuf_) {
      // If we have less than 4 byte, we cannot read the length. Wait for more data
      if (socket_->bytesAvailable() < 4) {
        break;
      }
      // Read the length of the next message
      QDataStream s(socket_);
      s >> expected_length_;

      // Receiving more than 128 MB is very unlikely
      // Flush the data and disconnect the client
      if (expected_length_ > 134217728) {
        qLog(Debug) << "Received invalid data, disconnect client";
        qLog(Debug) << "expected_length_ =" << expected_length_;
        socket_->close();
        return;
      }

      reading_protobuf_ = true;
    }

    // Read some of the message
    buffer_.append(socket_->read(static_cast<qint32>(expected_length_) - buffer_.size()));

    // Did we get everything?
    if (buffer_.size() == static_cast<qint32>(expected_length_)) {

      ParseMessage(buffer_);

      // Clear the buffer
      buffer_.clear();
      reading_protobuf_ = false;
    }
  }

}

void NetworkRemoteClient::ParseMessage(const QByteArray &data) {

  QProtobufSerializer serializer;
  networkremote::Message msg;
  if (!serializer.deserialize(&msg, data)) {
    qLog(Info) << "Couldn't parse data:" << serializer.lastErrorString();
    return;
  }

  if (msg.type() == networkremote::MsgTypeGadget::MsgType::CONNECT && use_auth_code_) {
    if (msg.requestConnect().authCode() != auth_code_) {
      DisconnectClient(networkremote::ReasonDisconnectGadget::ReasonDisconnect::Wrong_Auth_Code);
      return;
    }
    else {
      authenticated_ = true;
    }
  }

  if (msg.type() == networkremote::MsgTypeGadget::MsgType::CONNECT) {
    setDownloader(msg.requestConnect().hasDownloader() && msg.requestConnect().downloader());
    qLog(Debug) << "Downloader" << downloader_;
  }

  // Check if downloads are allowed
  if (msg.type() == networkremote::MsgTypeGadget::MsgType::DOWNLOAD_SONGS && !allow_downloads_) {
    DisconnectClient(networkremote::ReasonDisconnectGadget::ReasonDisconnect::Download_Forbidden);
    return;
  }

  if (msg.type() == networkremote::MsgTypeGadget::MsgType::DISCONNECT) {
    socket_->abort();
    qLog(Debug) << "Client disconnected";
    return;
  }

  // Check if the client has sent the correct auth code
  if (!authenticated_) {
    DisconnectClient(networkremote::ReasonDisconnectGadget::ReasonDisconnect::Not_Authenticated);
    return;
  }

  // Now parse the other data
  Q_EMIT Parse(msg);

}

void NetworkRemoteClient::DisconnectClient(const networkremote::ReasonDisconnectGadget::ReasonDisconnect reason) {

  networkremote::Message msg;
  msg.setType(networkremote::MsgTypeGadget::MsgType::DISCONNECT);

  networkremote::ResponseDisconnect response_disconnect;
  response_disconnect.setReasonDisconnect(reason);
  msg.setResponseDisconnect(response_disconnect);
  SendDataToClient(&msg);

  // Just close the connection. The next time the outgoing data creator sends a keep alive, the client will be deleted
  socket_->close();

}

// Sends data to client without check if authenticated
void NetworkRemoteClient::SendDataToClient(networkremote::Message *msg) {

  //msg->setVersion(msg);

  if (socket_->state() == QTcpSocket::ConnectedState) {
    // Serialize the message
    QProtobufSerializer serializer;
    const QByteArray data = serializer.serialize(msg);

    // Write the length of the data first
    QDataStream s(socket_);
    s << static_cast<qint32>(data.length());
    if (downloader_) {
      // Don't use QDataSteam for large files
      socket_->write(data.data(), data.length());
    }
    else {
      s.writeRawData(data.data(), data.length());
    }

    // Do NOT flush data here! If the client is already disconnected, it causes a SIGPIPE termination!!!
  }
  else {
    qDebug() << "Closed";
    socket_->close();
  }

}

void NetworkRemoteClient::SendData(networkremote::Message *msg) {

  if (authenticated_) {
    SendDataToClient(msg);
  }

}

QAbstractSocket::SocketState NetworkRemoteClient::State() const { return socket_->state(); }
