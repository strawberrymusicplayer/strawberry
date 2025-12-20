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
#include <QDateTime>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "qobuzservice.h"
#include "qobuzmetadatarequest.h"

namespace {
constexpr qint64 kNsecPerSec = 1000000000LL;
}

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

  const QJsonObject &json_obj = result.json_object;

  Song song;
  song.set_source(Song::Source::Qobuz);

  // Parse song ID
  QString song_id;
  if (json_obj["id"_L1].isString()) {
    song_id = json_obj["id"_L1].toString();
  }
  else {
    song_id = QString::number(json_obj["id"_L1].toInt());
  }
  song.set_song_id(song_id);

  // Parse basic track info
  if (json_obj.contains("title"_L1)) {
    song.set_title(json_obj["title"_L1].toString());
  }
  if (json_obj.contains("track_number"_L1)) {
    song.set_track(json_obj["track_number"_L1].toInt());
  }
  if (json_obj.contains("media_number"_L1)) {
    song.set_disc(json_obj["media_number"_L1].toInt());
  }
  if (json_obj.contains("duration"_L1)) {
    song.set_length_nanosec(json_obj["duration"_L1].toInt() * kNsecPerSec);
  }
  if (json_obj.contains("copyright"_L1)) {
    song.set_comment(json_obj["copyright"_L1].toString());
  }
  if (json_obj.contains("composer"_L1)) {
    QJsonValue value_composer = json_obj["composer"_L1];
    if (value_composer.isObject()) {
      QJsonObject obj_composer = value_composer.toObject();
      if (obj_composer.contains("name"_L1)) {
        song.set_composer(obj_composer["name"_L1].toString());
      }
    }
  }
  if (json_obj.contains("performer"_L1)) {
    QJsonValue value_performer = json_obj["performer"_L1];
    if (value_performer.isObject()) {
      QJsonObject obj_performer = value_performer.toObject();
      if (obj_performer.contains("name"_L1)) {
        song.set_performer(obj_performer["name"_L1].toString());
      }
    }
  }

  // Parse album info (includes artist, cover, genre)
  if (json_obj.contains("album"_L1)) {
    QJsonValue value_album = json_obj["album"_L1];
    if (value_album.isObject()) {
      QJsonObject obj_album = value_album.toObject();

      if (obj_album.contains("id"_L1)) {
        QString album_id;
        if (obj_album["id"_L1].isString()) {
          album_id = obj_album["id"_L1].toString();
        }
        else {
          album_id = QString::number(obj_album["id"_L1].toInt());
        }
        song.set_album_id(album_id);
      }

      if (obj_album.contains("title"_L1)) {
        song.set_album(obj_album["title"_L1].toString());
      }

      // Artist from album
      if (obj_album.contains("artist"_L1)) {
        QJsonValue value_artist = obj_album["artist"_L1];
        if (value_artist.isObject()) {
          QJsonObject obj_artist = value_artist.toObject();
          if (obj_artist.contains("id"_L1)) {
            QString artist_id;
            if (obj_artist["id"_L1].isString()) {
              artist_id = obj_artist["id"_L1].toString();
            }
            else {
              artist_id = QString::number(obj_artist["id"_L1].toInt());
            }
            song.set_artist_id(artist_id);
          }
          if (obj_artist.contains("name"_L1)) {
            song.set_artist(obj_artist["name"_L1].toString());
            song.set_albumartist(obj_artist["name"_L1].toString());
          }
        }
      }

      // Cover image
      if (obj_album.contains("image"_L1)) {
        QJsonValue value_image = obj_album["image"_L1];
        if (value_image.isObject()) {
          QJsonObject obj_image = value_image.toObject();
          if (obj_image.contains("large"_L1)) {
            QString cover_url = obj_image["large"_L1].toString();
            if (!cover_url.isEmpty()) {
              song.set_art_automatic(QUrl(cover_url));
            }
          }
        }
      }

      // Genre
      if (obj_album.contains("genre"_L1)) {
        QJsonValue value_genre = obj_album["genre"_L1];
        if (value_genre.isObject()) {
          QJsonObject obj_genre = value_genre.toObject();
          if (obj_genre.contains("name"_L1)) {
            song.set_genre(obj_genre["name"_L1].toString());
          }
        }
      }

      // Release date / year
      if (obj_album.contains("released_at"_L1)) {
        qint64 released_at = obj_album["released_at"_L1].toVariant().toLongLong();
        if (released_at > 0) {
          QDateTime datetime = QDateTime::fromSecsSinceEpoch(released_at);
          song.set_year(datetime.date().year());
        }
      }
    }
  }

  song.set_valid(true);

  qLog(Debug) << "Qobuz: Track metadata received for" << track_id
              << "- title:" << song.title()
              << "- artist:" << song.artist()
              << "- album:" << song.album()
              << "- genre:" << song.genre();

  Q_EMIT MetadataReceived(track_id, song);

}

void QobuzMetadataRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Qobuz:" << error_message;
  if (debug_output.isValid()) qLog(Debug) << debug_output;

}
