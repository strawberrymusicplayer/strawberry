/*
 * Strawberry Music Player
 * Copyright 2021-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include "core/networkaccessmanager.h"
#include "core/taskmanager.h"
#include "core/iconloader.h"
#include "playlistparsers/playlistparser.h"
#include "somafmservice.h"
#include "radiochannel.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kApiChannelsUrl[] = "https://somafm.com/channels.json";
}  // namespace

SomaFMService::SomaFMService(const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::SomaFM, u"SomaFM"_s, IconLoader::Load(u"somafm"_s), task_manager, network, parent) {}

SomaFMService::~SomaFMService() {
  Abort();
}

QUrl SomaFMService::Homepage() { return QUrl(u"https://somafm.com/"_s); }
QUrl SomaFMService::Donate() { return QUrl(u"https://somafm.com/support/"_s); }

void SomaFMService::Abort() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

  channels_.clear();

}

void SomaFMService::GetChannels() {

  Abort();

  const QUrl url(QString::fromLatin1(kApiChannelsUrl));
  QNetworkRequest network_request(url);
  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  const int task_id = task_manager_->StartTask(tr("Getting %1 channels").arg(name_));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, task_id]() { GetChannelsReply(reply, task_id); });

}

void SomaFMService::GetChannelsReply(QNetworkReply *reply, const int task_id) {

  if (replies_.contains(reply)) replies_.removeAll(reply);
  reply->deleteLater();

  const QJsonObject object = ExtractJsonObj(reply);
  if (object.isEmpty()) {
    task_manager_->SetTaskFinished(task_id);
    Q_EMIT NewChannels();
    return;
  }

  if (!object.contains("channels"_L1) || !object["channels"_L1].isArray()) {
    Error(u"Missing JSON channels array."_s, object);
    task_manager_->SetTaskFinished(task_id);
    Q_EMIT NewChannels();
    return;
  }
  const QJsonArray array_channels = object["channels"_L1].toArray();

  RadioChannelList channels;
  for (const QJsonValue &value_channel : array_channels) {
    if (!value_channel.isObject()) continue;
    QJsonObject obj_channel = value_channel.toObject();
    if (!obj_channel.contains("title"_L1) || !obj_channel.contains("image"_L1)) {
      continue;
    }
    QString name = obj_channel["title"_L1].toString();
    QString image = obj_channel["image"_L1].toString();
    const QJsonArray playlists = obj_channel["playlists"_L1].toArray();
    for (const QJsonValue &playlist : playlists) {
      if (!playlist.isObject()) continue;
      QJsonObject obj_playlist = playlist.toObject();
      if (!obj_playlist.contains("url"_L1) || !obj_playlist.contains("quality"_L1)) {
        continue;
      }
      RadioChannel channel;
      QString quality = obj_playlist["quality"_L1].toString();
      if (quality != "highest"_L1) continue;
      channel.source = source_;
      channel.name = name;
      channel.url.setUrl(obj_playlist["url"_L1].toString());
      channel.thumbnail_url.setUrl(image);
      if (obj_playlist.contains("format"_L1)) {
        channel.name.append(QLatin1Char(' ') + obj_playlist[QLatin1String("format")].toString().toUpper());
      }
      channels << channel;
    }
  }

  if (channels.isEmpty()) {
    task_manager_->SetTaskFinished(task_id);
    Q_EMIT NewChannels();
  }
  else {
    for (const RadioChannel &channel : std::as_const(channels)) {
      GetStreamUrl(task_id, channel);
    }
  }

}

void SomaFMService::GetStreamUrl(const int task_id, const RadioChannel &channel) {

  QNetworkRequest network_request(channel.url);
  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, task_id, channel]() { GetStreamUrlsReply(reply, task_id, channel); });

}

void SomaFMService::GetStreamUrlsReply(QNetworkReply *reply, const int task_id, RadioChannel channel) {  // clazy:exclude=function-args-by-ref

  if (replies_.contains(reply)) replies_.removeAll(reply);
  reply->deleteLater();

  PlaylistParser parser(nullptr, nullptr);
  SongList songs = parser.LoadFromDevice(reply);
  if (!songs.isEmpty()) {
    channel.url = songs.first().url();
  }

  channels_ << channel;

  if (replies_.isEmpty()) {
    task_manager_->SetTaskFinished(task_id);
    Q_EMIT NewChannels(channels_);
    channels_.clear();
  }

}
