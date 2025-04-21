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

#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "core/iconloader.h"
#include "radioparadiseservice.h"
#include "radiochannel.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kApiChannelsUrl[] = "https://api.radioparadise.com/api/list_streams";
}  // namespace

RadioParadiseService::RadioParadiseService(const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : RadioService(Song::Source::RadioParadise, u"Radio Paradise"_s, IconLoader::Load(u"radioparadise"_s), task_manager, network, parent) {}

QUrl RadioParadiseService::Homepage() { return QUrl(u"https://radioparadise.com/"_s); }
QUrl RadioParadiseService::Donate() { return QUrl(u"https://payments.radioparadise.com/rp2s-content.php?name=Support&file=support"_s); }

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

  QNetworkRequest network_request(QUrl(QString::fromLatin1(kApiChannelsUrl)));
  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  const int task_id = task_manager_->StartTask(tr("Getting %1 channels").arg(name_));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, task_id]() { GetChannelsReply(reply, task_id); });

}

void RadioParadiseService::GetChannelsReply(QNetworkReply *reply, const int task_id) {

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
    const QJsonObject obj_channel = value_channel.toObject();
    if (!obj_channel.contains("chan_name"_L1) || !obj_channel.contains("streams"_L1)) {
      continue;
    }
    const QString name = obj_channel["chan_name"_L1].toString();
    const QJsonValue value_streams = obj_channel["streams"_L1];
    if (!value_streams.isArray()) {
      continue;
    }
    const QJsonArray array_streams = obj_channel["streams"_L1].toArray();
    for (const QJsonValue &value_stream : array_streams) {
      if (!value_stream.isObject()) continue;
      const QJsonObject obj_stream = value_stream.toObject();
      if (!obj_stream.contains("label"_L1) || !obj_stream.contains("url"_L1)) {
        continue;
      }
      const QString label = obj_stream["label"_L1].toString();
      QString url = obj_stream["url"_L1].toString();
      static const QRegularExpression regex_url_schema(u"^[0-9a-zA-Z]*:\\/\\/"_s, QRegularExpression::CaseInsensitiveOption);
      if (!url.contains(regex_url_schema)) {
        url.prepend("https://"_L1);
      }
      RadioChannel channel;
      channel.source = source_;
      channel.name = name + " - "_L1 + label;
      channel.url.setUrl(url);
      channels << channel;
    }
  }

  task_manager_->SetTaskFinished(task_id);

  Q_EMIT NewChannels(channels);

}
