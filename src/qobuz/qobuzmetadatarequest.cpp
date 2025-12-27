/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QObject>
#include <QString>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "qobuzservice.h"
#include "qobuzmetadatarequest.h"

using namespace Qt::Literals::StringLiterals;

QobuzMetadataRequest::QobuzMetadataRequest(QobuzService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QobuzBaseRequest(service, network, parent) {}

void QobuzMetadataRequest::FetchTrackMetadata(const QString &track_id) {

  if (!authenticated()) {
    Q_EMIT MetadataFailure(track_id, tr("Not authenticated"));
    return;
  }

  if (track_id.isEmpty()) {
    Q_EMIT MetadataFailure(track_id, tr("No track ID"));
    return;
  }

  ParamList params = ParamList() << Param(u"track_id"_s, track_id);

  QNetworkReply *reply = CreateRequest(u"track/get"_s, params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, track_id]() {
    TrackMetadataReceived(reply, track_id);
  });

}

void QobuzMetadataRequest::TrackMetadataReceived(QNetworkReply *reply, const QString &track_id) {

  if (!replies_.contains(reply)) {
    qLog(Debug) << "Qobuz: Reply not in replies_ list for track" << track_id;
    return;
  }
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  JsonObjectResult result = ParseJsonObject(reply);
  if (result.error_code != JsonBaseRequest::ErrorCode::Success) {
    Error(result.error_message);
    Q_EMIT MetadataFailure(track_id, result.error_message);
    return;
  }

  const QJsonObject &json_object = result.json_object;

  QString genre;

  // Genre is in the album object within the track response
  if (json_object.contains("album"_L1)) {
    QJsonValue value_album = json_object["album"_L1];
    if (value_album.isObject()) {
      QJsonObject obj_album = value_album.toObject();
      if (obj_album.contains("genre"_L1)) {
        QJsonValue value_genre = obj_album["genre"_L1];
        if (value_genre.isObject()) {
          QJsonObject obj_genre = value_genre.toObject();
          if (obj_genre.contains("name"_L1)) {
            genre = obj_genre["name"_L1].toString();
          }
        }
      }
    }
  }

  qLog(Debug) << "Qobuz: Track metadata received for" << track_id << "- genre:" << (genre.isEmpty() ? u"(empty)"_s : genre);

  Q_EMIT MetadataReceived(track_id, genre);

}

void QobuzMetadataRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Qobuz:" << error_message;
  if (debug_output.isValid()) qLog(Debug) << debug_output;

}
