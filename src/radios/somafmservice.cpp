/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/taskmanager.h"
#include "core/iconloader.h"
#include "playlistparsers/playlistparser.h"
#include "somafmservice.h"
#include "radiochannel.h"

const char *SomaFMService::kApiChannelsUrl = "https://somafm.com/channels.json";

SomaFMService::SomaFMService(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::SomaFM, "SomaFM", IconLoader::Load("somafm"), app, network, parent) {}

SomaFMService::~SomaFMService() {
  Abort();
}

QUrl SomaFMService::Homepage() { return QUrl("https://somafm.com/"); }
QUrl SomaFMService::Donate() { return QUrl("https://somafm.com/support/"); }

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

  QUrl url(kApiChannelsUrl);
  QNetworkRequest req(url);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  const int task_id = app_->task_manager()->StartTask(tr("Getting %1 channels").arg(name_));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, task_id]() { GetChannelsReply(reply, task_id); });

}

void SomaFMService::GetChannelsReply(QNetworkReply *reply, const int task_id) {

  if (replies_.contains(reply)) replies_.removeAll(reply);
  reply->deleteLater();

  QJsonObject object = ExtractJsonObj(reply);
  if (object.isEmpty()) {
    app_->task_manager()->SetTaskFinished(task_id);
    emit NewChannels();
    return;
  }

  if (!object.contains("channels") || !object["channels"].isArray()) {
    Error("Missing JSON channels array.", object);
    app_->task_manager()->SetTaskFinished(task_id);
    emit NewChannels();
    return;
  }
  QJsonArray array_channels = object["channels"].toArray();

  RadioChannelList channels;
  for (const QJsonValueRef value_channel : array_channels) {
    if (!value_channel.isObject()) continue;
    QJsonObject obj_channel = value_channel.toObject();
    if (!obj_channel.contains("title") || !obj_channel.contains("image")) {
      continue;
    }
    QString name = obj_channel["title"].toString();
    QString image = obj_channel["image"].toString();
    QJsonArray playlists = obj_channel["playlists"].toArray();
    for (const QJsonValueRef playlist : playlists) {
      if (!playlist.isObject()) continue;
      QJsonObject obj_playlist = playlist.toObject();
      if (!obj_playlist.contains("url") || !obj_playlist.contains("quality")) {
        continue;
      }
      RadioChannel channel;
      QString quality = obj_playlist["quality"].toString();
      if (quality != "highest") continue;
      channel.source = source_;
      channel.name = name;
      channel.url.setUrl(obj_playlist["url"].toString());
      channel.thumbnail_url.setUrl(image);
      if (obj_playlist.contains("format")) {
        channel.name.append(" " + obj_playlist["format"].toString().toUpper());
      }
      channels << channel;
    }
  }

  if (channels.isEmpty()) {
    app_->task_manager()->SetTaskFinished(task_id);
    emit NewChannels();
  }
  else {
    for (const RadioChannel &channel : channels) {
      GetStreamUrl(task_id, channel);
    }
  }

}

void SomaFMService::GetStreamUrl(const int task_id, const RadioChannel &channel) {

  QNetworkRequest req(channel.url);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, task_id, channel]() { GetStreamUrlsReply(reply, task_id, channel); });

}

void SomaFMService::GetStreamUrlsReply(QNetworkReply *reply, const int task_id, RadioChannel channel) {  // clazy:exclude=function-args-by-ref

  if (replies_.contains(reply)) replies_.removeAll(reply);
  reply->deleteLater();

  PlaylistParser parser;
  SongList songs = parser.LoadFromDevice(reply);
  if (!songs.isEmpty()) {
    channel.url = songs.first().url();
  }

  channels_ << channel;

  if (replies_.isEmpty()) {
    app_->task_manager()->SetTaskFinished(task_id);
    emit NewChannels(channels_);
    channels_.clear();
  }

}
