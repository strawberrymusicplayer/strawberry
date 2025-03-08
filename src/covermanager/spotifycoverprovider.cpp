/*
 * Strawberry Music Player
 * Copyright 2020-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "spotify/spotifyservice.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "spotifycoverprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kApiUrl[] = "https://api.spotify.com/v1";
constexpr int kLimit = 10;
}  // namespace

SpotifyCoverProvider::SpotifyCoverProvider(const SpotifyServicePtr service, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Spotify"_s, true, true, 2.5, true, true, network, parent),
      service_(service) {}

bool SpotifyCoverProvider::authenticated() const {

  return service_->authenticated();

}

bool SpotifyCoverProvider::use_authorization_header() const {

  return true;

}

QByteArray SpotifyCoverProvider::authorization_header() const {

  return service_->authorization_header();

}

void SpotifyCoverProvider::ClearSession() {

  service_->ClearSession();

}

bool SpotifyCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (!service_->authenticated()) return false;

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString type;
  QString extract;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    type = "track"_L1;
    extract = "tracks"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    type = "album"_L1;
    extract = "albums"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  const ParamList params = ParamList() << Param(u"q"_s, query)
                                       << Param(u"type"_s, type)
                                       << Param(u"limit"_s, QString::number(kLimit));

  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(kApiUrl) + u"/search"_s), params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, extract]() { HandleSearchReply(reply, id, extract); });

  return true;

}

void SpotifyCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

JsonBaseRequest::JsonObjectResult SpotifyCoverProvider::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains("error"_L1) && json_object["error"_L1].isObject()) {
        const QJsonObject object_error = json_object["error"_L1].toObject();
        if (object_error.contains("status"_L1) && object_error.contains("message"_L1)) {
          const int status = object_error["status"_L1].toInt();
          const QString message = object_error["message"_L1].toString();
          result.error_code = ErrorCode::APIError;
          result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(status);
        }
      }
      else {
        result.json_object = json_document.object();
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::APIError) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else if (result.http_status_code != 200) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
    service_->ClearSession();
  }

  return result;

}

void SpotifyCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &extract) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverProviderSearchResults results;
  const QScopeGuard search_finished = qScopeGuard([this, id, &results]() { Q_EMIT SearchFinished(id, results); });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains(extract) || !json_object[extract].isObject()) {
    Error(QStringLiteral("Json object is missing %1 object.").arg(extract), json_object);
    return;
  }
  json_object = json_object[extract].toObject();

  if (!json_object.contains("items"_L1) || !json_object["items"_L1].isArray()) {
    Error(QStringLiteral("%1 object is missing items array.").arg(extract), json_object);
    return;
  }

  const QJsonArray array_items = json_object["items"_L1].toArray();
  if (array_items.isEmpty()) {
    return;
  }

  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      continue;
    }
    const QJsonObject object_item = value_item.toObject();

    QJsonObject obj_album = object_item;
    if (object_item.contains("album"_L1) && object_item["album"_L1].isObject()) {
      obj_album = object_item["album"_L1].toObject();
    }

    if (!obj_album.contains("artists"_L1) || !obj_album.contains("name"_L1) || !obj_album.contains("images"_L1) || !obj_album["artists"_L1].isArray() || !obj_album["images"_L1].isArray()) {
      continue;
    }
    const QJsonArray array_artists = obj_album["artists"_L1].toArray();
    const QJsonArray array_images = obj_album["images"_L1].toArray();
    const QString album = obj_album["name"_L1].toString();

    QStringList artists;
    for (const QJsonValue &value_artist : array_artists) {
      if (!value_artist.isObject()) continue;
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains("name"_L1)) continue;
      artists << obj_artist["name"_L1].toString();
    }

    for (const QJsonValue &value_image : array_images) {
      if (!value_image.isObject()) continue;
      const QJsonObject object_image = value_image.toObject();
      if (!object_image.contains("url"_L1) || !object_image.contains("width"_L1) || !object_image.contains("height"_L1)) continue;
      const int width = object_image["width"_L1].toInt();
      const int height = object_image["height"_L1].toInt();
      if (width < 300 || height < 300) continue;
      const QUrl url(object_image["url"_L1].toString());
      CoverProviderSearchResult result;
      result.album = album;
      result.image_url = url;
      result.image_size = QSize(width, height);
      if (!artists.isEmpty()) result.artist = artists.first();
      results << result;
    }

  }

}

void SpotifyCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Spotify:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
