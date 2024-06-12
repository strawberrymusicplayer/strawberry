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

namespace {
constexpr char kApiChannelsUrl[] = "https://somafm.com/channels.json";
}

SomaFMService::SomaFMService(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::SomaFM, QStringLiteral("SomaFM"), IconLoader::Load(QStringLiteral("somafm")), app, network, parent) {}

SomaFMService::~SomaFMService() {
  Abort();
}

QUrl SomaFMService::Homepage() { return QUrl(QStringLiteral("https://somafm.com/")); }
QUrl SomaFMService::Donate() { return QUrl(QStringLiteral("https://somafm.com/support/")); }

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

  QUrl url(QString::fromLatin1(kApiChannelsUrl));
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

  if (!object.contains(QLatin1String("channels")) || !object[QLatin1String("channels")].isArray()) {
    Error(QStringLiteral("Missing JSON channels array."), object);
    app_->task_manager()->SetTaskFinished(task_id);
    emit NewChannels();
    return;
  }
  QJsonArray array_channels = object[QLatin1String("channels")].toArray();

  RadioChannelList channels;
  for (const QJsonValueRef value_channel : array_channels) {
    if (!value_channel.isObject()) continue;
    QJsonObject obj_channel = value_channel.toObject();
    if (!obj_channel.contains(QLatin1String("title")) || !obj_channel.contains(QLatin1String("image"))) {
      continue;
    }
    QString name = obj_channel[QLatin1String("title")].toString();
    QString image = obj_channel[QLatin1String("image")].toString();
    QJsonArray playlists = obj_channel[QLatin1String("playlists")].toArray();
    for (const QJsonValueRef playlist : playlists) {
      if (!playlist.isObject()) continue;
      QJsonObject obj_playlist = playlist.toObject();
      if (!obj_playlist.contains(QLatin1String("url")) || !obj_playlist.contains(QLatin1String("quality"))) {
        continue;
      }
      RadioChannel channel;
      QString quality = obj_playlist[QLatin1String("quality")].toString();
      if (quality != QStringLiteral("highest")) continue;
      channel.source = source_;
      channel.name = name;
      channel.url.setUrl(obj_playlist[QLatin1String("url")].toString());
      channel.thumbnail_url.setUrl(image);
      if (obj_playlist.contains(QLatin1String("format"))) {
        channel.name.append(QLatin1Char(' ') + obj_playlist[QLatin1String("format")].toString().toUpper());
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
