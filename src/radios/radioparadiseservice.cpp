/*
 * Strawberry Music Player
 * Copyright 2021-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include "radioparadiseservice.h"
#include "radiochannel.h"

const char *RadioParadiseService::kApiChannelsUrl = "https://api.radioparadise.com/api/list_streams";

RadioParadiseService::RadioParadiseService(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::RadioParadise, "Radio Paradise", IconLoader::Load("radioparadise"), app, network, parent) {}

QUrl RadioParadiseService::Homepage() { return QUrl("https://radioparadise.com/"); }
QUrl RadioParadiseService::Donate() { return QUrl("https://payments.radioparadise.com/rp2s-content.php?name=Support&file=support"); }

void RadioParadiseService::Abort() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

  channels_.clear();

}

void RadioParadiseService::GetChannels() {

  Abort();

  QUrl url(kApiChannelsUrl);
  QNetworkRequest req(url);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  const int task_id = app_->task_manager()->StartTask(tr("Getting %1 channels").arg(name_));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, task_id]() { GetChannelsReply(reply, task_id); });

}

void RadioParadiseService::GetChannelsReply(QNetworkReply *reply, const int task_id) {

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
    if (!obj_channel.contains("chan_name") || !obj_channel.contains("streams")) {
      continue;
    }
    QString name = obj_channel["chan_name"].toString();
    QJsonValue value_streams = obj_channel["streams"];
    if (!value_streams.isArray()) {
      continue;
    }
    QJsonArray array_streams = obj_channel["streams"].toArray();
    for (const QJsonValueRef value_stream : array_streams) {
      if (!value_stream.isObject()) continue;
      QJsonObject obj_stream = value_stream.toObject();
      if (!obj_stream.contains("label") || !obj_stream.contains("url")) {
        continue;
      }
      QString label = obj_stream["label"].toString();
      QString url = obj_stream["url"].toString();
      if (!url.contains(QRegularExpression("^[0-9a-zA-Z]*:\\/\\/", QRegularExpression::CaseInsensitiveOption))) {
        url.prepend("https://");
      }
      RadioChannel channel;
      channel.source = source_;
      channel.name = name + " - " + label;
      channel.url.setUrl(url);
      channels << channel;
    }
  }

  app_->task_manager()->SetTaskFinished(task_id);

  emit NewChannels(channels);

}
