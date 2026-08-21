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

#include <QHostAddress>
#include <QNetworkInterface>
#include "networkremotesettings.h"
#include "core/logging.h"


const char *NetworkRemoteSettings::kSettingsGroup = "NetworkRemote";

NetworkRemoteSettings::NetworkRemoteSettings()
    : enabled_(false),
      remote_port_(8888),
    playlist_size_(50 ){}

QString NetworkRemoteSettings::cached_token_;
int NetworkRemoteSettings::cached_playlist_size_ = 50;

NetworkRemoteSettings::~NetworkRemoteSettings() {}

void NetworkRemoteSettings::Load() {  
  settings_.beginGroup(NetworkRemoteSettings::kSettingsGroup);
  if (!settings_.contains("useRemote")) {
    qLog(Debug) << "First time run the Network Remote";
    settings_.setValue("useRemote", false);
    settings_.setValue("remotePort", 8888);
    settings_.setValue("remoteToken", QString());
    settings_.setValue("playlistSize", 50);
  }
  else {
    enabled_ = settings_.value("useRemote").toBool();
    remote_port_ = settings_.value("remotePort").toInt();
    remote_token_ = settings_.value("remoteToken").toString();
    playlist_size_ = settings_.value("playlistSize", 50).toInt();
  }

  if (remote_port_ < kMinPort || remote_port_ > kMaxPort) {
      qLog(Warning) << "Invalid NetworkRemote port" << remote_port_
                    << "in settings, falling back to" << kDefaultPort;
      remote_port_ = kDefaultPort;
  }

  if (playlist_size_ < kMinUpcomingRows || playlist_size_ > kMaxUpcomingRows) {
      qLog(Warning) << "Invalid NetworkRemote playlist size" << playlist_size_
                    << "in settings, clamping to valid range";
      playlist_size_ = std::clamp(playlist_size_, kMinUpcomingRows, kMaxUpcomingRows);
  }
  settings_.endGroup();
  qLog(Debug) << "QSettings Loaded ++++++++++++++++";
}

void NetworkRemoteSettings::Save() {
  settings_.beginGroup(NetworkRemoteSettings::kSettingsGroup);
  settings_.setValue("useRemote", enabled_);
  settings_.setValue("remotePort", remote_port_);
  settings_.setValue("remoteToken", remote_token_);
  settings_.setValue("playlistSize", playlist_size_);
  settings_.endGroup();
  settings_.sync();
  qLog(Debug) << "Saving QSettings ++++++++++++++++";
}

bool NetworkRemoteSettings::UseRemote() const {
  return enabled_;
}

int NetworkRemoteSettings::GetPort() const {
  return remote_port_;
}

QString NetworkRemoteSettings::GetToken() const {
    return remote_token_;
}

void NetworkRemoteSettings::SetUseRemote(bool useRemote) {
  enabled_ = useRemote;
}


void NetworkRemoteSettings::SetPort(int port) {
  remote_port_ = std::clamp(port, kMinPort, kMaxPort);
}

void NetworkRemoteSettings::SetToken(const QString &token) {
    remote_token_ = token;
}

QString NetworkRemoteSettings::CurrentToken() {
    return cached_token_;
}

int NetworkRemoteSettings::GetPlaylistSize() const {
    return playlist_size_;
}

void NetworkRemoteSettings::SetPlaylistSize(int size) {
    playlist_size_ = std::clamp(size, kMinUpcomingRows, kMaxUpcomingRows);
}

int NetworkRemoteSettings::CurrentPlaylistSize() {
    return cached_playlist_size_;
}

void NetworkRemoteSettings::RefreshCache() {
    NetworkRemoteSettings settings;
    settings.Load();
    cached_token_ = settings.GetToken();
    cached_playlist_size_ = settings.GetPlaylistSize();
}

