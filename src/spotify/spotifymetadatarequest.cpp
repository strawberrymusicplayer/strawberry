/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QJsonArray>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "spotifyservice.h"
#include "spotifymetadatarequest.h"

using namespace Qt::Literals::StringLiterals;

SpotifyMetadataRequest::SpotifyMetadataRequest(SpotifyService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : SpotifyBaseRequest(service, network, parent) {}

void SpotifyMetadataRequest::FetchTrackMetadata(const QString &track_id) {

  if (!authenticated()) {
    Q_EMIT MetadataFailure(track_id, tr("Not authenticated"));
    return;
  }

  if (track_id.isEmpty()) {
    Q_EMIT MetadataFailure(track_id, tr("No track ID"));
    return;
  }

  QNetworkReply *reply = CreateRequest(u"tracks/"_s + track_id, ParamList());
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, track_id]() {
    TrackMetadataReceived(reply, track_id);
  });

}

void SpotifyMetadataRequest::TrackMetadataReceived(QNetworkReply *reply, const QString &track_id) {

  if (!replies_.contains(reply)) return;
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

  // Extract artist ID from track response
  QString artist_id;
  if (json_object.contains("artists"_L1) && json_object["artists"_L1].isArray()) {
    const QJsonArray array_artists = json_object["artists"_L1].toArray();
    if (!array_artists.isEmpty()) {
      const QJsonObject obj_artist = array_artists.first().toObject();
      if (obj_artist.contains("id"_L1)) {
        artist_id = obj_artist["id"_L1].toString();
      }
    }
  }

  if (artist_id.isEmpty()) {
    Q_EMIT MetadataFailure(track_id, tr("No artist ID in track response"));
    return;
  }

  // Now fetch artist metadata to get genre
  QNetworkReply *artist_reply = CreateRequest(u"artists/"_s + artist_id, ParamList());
  QObject::connect(artist_reply, &QNetworkReply::finished, this, [this, artist_reply, track_id]() {
    ArtistMetadataReceived(artist_reply, track_id);
  });

}

void SpotifyMetadataRequest::ArtistMetadataReceived(QNetworkReply *reply, const QString &track_id) {

  if (!replies_.contains(reply)) return;
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
  if (json_object.contains("genres"_L1) && json_object["genres"_L1].isArray()) {
    const QJsonArray array_genres = json_object["genres"_L1].toArray();
    if (!array_genres.isEmpty()) {
      genre = array_genres.first().toString();
    }
  }

  Q_EMIT MetadataReceived(track_id, genre);

}

void SpotifyMetadataRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Spotify:" << error_message;
  if (debug_output.isValid()) qLog(Debug) << debug_output;

}
