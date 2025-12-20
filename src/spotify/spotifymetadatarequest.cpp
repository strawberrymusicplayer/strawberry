/*
 * Strawberry Music Player
 * Copyright 2025-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QUrl>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "spotifyservice.h"
#include "spotifymetadatarequest.h"

namespace {
constexpr qint64 kNsecPerMsec = 1000000LL;
}

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

  const QJsonObject &json_obj = result.json_object;

  Song song;
  song.set_source(Song::Source::Spotify);

  // Parse song ID and URI
  if (json_obj.contains("id"_L1)) {
    song.set_song_id(json_obj["id"_L1].toString());
  }
  if (json_obj.contains("uri"_L1)) {
    song.set_url(QUrl(json_obj["uri"_L1].toString()));
  }

  // Parse basic track info
  if (json_obj.contains("name"_L1)) {
    song.set_title(json_obj["name"_L1].toString());
  }
  if (json_obj.contains("track_number"_L1)) {
    song.set_track(json_obj["track_number"_L1].toInt());
  }
  if (json_obj.contains("disc_number"_L1)) {
    song.set_disc(json_obj["disc_number"_L1].toInt());
  }
  if (json_obj.contains("duration_ms"_L1)) {
    song.set_length_nanosec(json_obj["duration_ms"_L1].toVariant().toLongLong() * kNsecPerMsec);
  }

  // Extract artist info
  QString artist_id;
  if (json_obj.contains("artists"_L1) && json_obj["artists"_L1].isArray()) {
    const QJsonArray array_artists = json_obj["artists"_L1].toArray();
    if (!array_artists.isEmpty()) {
      const QJsonObject obj_artist = array_artists.first().toObject();
      if (obj_artist.contains("id"_L1)) {
        artist_id = obj_artist["id"_L1].toString();
        song.set_artist_id(artist_id);
      }
      if (obj_artist.contains("name"_L1)) {
        song.set_artist(obj_artist["name"_L1].toString());
      }
    }
  }

  // Extract album info
  if (json_obj.contains("album"_L1) && json_obj["album"_L1].isObject()) {
    QJsonObject obj_album = json_obj["album"_L1].toObject();
    if (obj_album.contains("id"_L1)) {
      song.set_album_id(obj_album["id"_L1].toString());
    }
    if (obj_album.contains("name"_L1)) {
      song.set_album(obj_album["name"_L1].toString());
    }
    // Cover image - prefer larger images
    if (obj_album.contains("images"_L1) && obj_album["images"_L1].isArray()) {
      const QJsonArray array_images = obj_album["images"_L1].toArray();
      for (const QJsonValue &value : array_images) {
        if (!value.isObject()) continue;
        QJsonObject obj_image = value.toObject();
        if (!obj_image.contains("url"_L1) || !obj_image.contains("width"_L1) || !obj_image.contains("height"_L1)) continue;
        int width = obj_image["width"_L1].toInt();
        int height = obj_image["height"_L1].toInt();
        if (width >= 300 && height >= 300) {
          song.set_art_automatic(QUrl(obj_image["url"_L1].toString()));
          break;
        }
      }
    }
    // Album artist
    if (obj_album.contains("artists"_L1) && obj_album["artists"_L1].isArray()) {
      const QJsonArray array_album_artists = obj_album["artists"_L1].toArray();
      if (!array_album_artists.isEmpty()) {
        const QJsonObject obj_album_artist = array_album_artists.first().toObject();
        if (obj_album_artist.contains("name"_L1)) {
          song.set_albumartist(obj_album_artist["name"_L1].toString());
        }
      }
    }
    // Release date
    if (obj_album.contains("release_date"_L1)) {
      QString release_date = obj_album["release_date"_L1].toString();
      if (release_date.length() >= 4) {
        song.set_year(release_date.left(4).toInt());
      }
    }
  }

  song.set_valid(true);

  if (artist_id.isEmpty()) {
    // No artist ID - emit what we have without genre
    qLog(Debug) << "Spotify: Track metadata received for" << track_id << "(no artist ID for genre lookup)";
    Q_EMIT MetadataReceived(track_id, song);
    return;
  }

  // Store partial song and fetch artist metadata for genre
  pending_songs_[track_id] = song;

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

  // Retrieve the stored partial song
  if (!pending_songs_.contains(track_id)) {
    Q_EMIT MetadataFailure(track_id, tr("No pending song for track ID"));
    return;
  }
  Song song = pending_songs_.take(track_id);

  JsonObjectResult result = ParseJsonObject(reply);
  if (result.error_code != JsonBaseRequest::ErrorCode::Success) {
    // Still emit the song even without genre
    qLog(Warning) << "Spotify: Failed to get artist metadata for genre:" << result.error_message;
    Q_EMIT MetadataReceived(track_id, song);
    return;
  }

  const QJsonObject &json_object = result.json_object;

  // Add genre from artist
  if (json_object.contains("genres"_L1) && json_object["genres"_L1].isArray()) {
    const QJsonArray array_genres = json_object["genres"_L1].toArray();
    if (!array_genres.isEmpty()) {
      song.set_genre(array_genres.first().toString());
    }
  }

  qLog(Debug) << "Spotify: Track metadata received for" << track_id
              << "- title:" << song.title()
              << "- artist:" << song.artist()
              << "- album:" << song.album()
              << "- genre:" << song.genre();

  Q_EMIT MetadataReceived(track_id, song);

}

void SpotifyMetadataRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Spotify:" << error_message;
  if (debug_output.isValid()) qLog(Debug) << debug_output;

}
