/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>

#include <QLocale>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"

#include "jsoncoverprovider.h"
#include "albumcoverfetcher.h"
#include "lastfmcoverprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrl[] = "https://ws.audioscrobbler.com/2.0/";
constexpr char kApiKey[] = "211990b4c96782c05d1536e7219eb56e";
constexpr char kSecret[] = "80fd738f49596e9709b1bf9319c444a8";
}  // namespace

LastFmCoverProvider::LastFmCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Last.fm"_s, true, false, 1.0, true, false, network, parent) {}

bool LastFmCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString method;
  QString type;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    method = "track.search"_L1;
    type = "track"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    method = "album.search"_L1;
    type = "album"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  ParamList params = ParamList() << Param(u"api_key"_s, QLatin1String(kApiKey))
                                 << Param(u"lang"_s, QLocale().name().left(2).toLower())
                                 << Param(u"method"_s, method)
                                 << Param(type, query);

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  QString data_to_sign;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
    data_to_sign += param.first + param.second;
  }
  data_to_sign += QLatin1String(kSecret);

  QByteArray const digest = QCryptographicHash::hash(data_to_sign.toUtf8(), QCryptographicHash::Md5);
  QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, u'0').toLower();

  url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(u"api_sig"_s)), QString::fromLatin1(QUrl::toPercentEncoding(signature)));
  url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(u"format"_s)), QString::fromLatin1(QUrl::toPercentEncoding(u"json"_s)));

  QNetworkReply *reply = CreatePostRequest(QUrl(QLatin1String(kUrl)), url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, type]() { QueryFinished(reply, id, type); });

  return true;

}

JsonBaseRequest::JsonObjectResult LastFmCoverProvider::ParseJsonObject(QNetworkReply *reply) {

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
      if (json_object.contains("error"_L1) && json_object.contains("message"_L1)) {
        const int error = json_object["error"_L1].toInt();
        const QString message = json_object["message"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(error);
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

  return result;

}

void LastFmCoverProvider::QueryFinished(QNetworkReply *reply, const int id, const QString &type) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverProviderSearchResults results;
  const QScopeGuard end_search = qScopeGuard([this, id, &results]() { Q_EMIT SearchFinished(id, results); });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  QJsonValue value_results;
  if (json_object.contains("results"_L1)) {
    value_results = json_object["results"_L1];
  }
  else if (json_object.contains("error"_L1) && json_object.contains("message"_L1)) {
    const int error = json_object["error"_L1].toInt();
    const QString message = json_object["message"_L1].toString();
    Error(QStringLiteral("Error: %1: %2").arg(QString::number(error), message));
    return;
  }
  else {
    Error(u"Json reply is missing results."_s, json_object);
    return;
  }

  if (!value_results.isObject()) {
    Error(u"Json results is not a object."_s, value_results);
    return;
  }

  const QJsonObject object_results = value_results.toObject();
  if (object_results.isEmpty()) {
    Error(u"Json results object is empty."_s, value_results);
    return;
  }

  QJsonValue value_matches;

  if (type == "album"_L1) {
    if (object_results.contains("albummatches"_L1)) {
      value_matches = object_results["albummatches"_L1];
    }
    else {
      Error(u"Json results object is missing albummatches."_s, object_results);
      return;
    }
  }
  else if (type == "track"_L1) {
    if (object_results.contains("trackmatches"_L1)) {
      value_matches = object_results["trackmatches"_L1];
    }
    else {
      Error(u"Json results object is missing trackmatches."_s, object_results);
      return;
    }
  }

  if (!value_matches.isObject()) {
    Error(u"Json albummatches or trackmatches is not an object."_s, value_matches);
    return;
  }

  const QJsonObject object_matches = value_matches.toObject();
  if (object_matches.isEmpty()) {
    Error(u"Json albummatches or trackmatches object is empty."_s, value_matches);
    return;
  }

  if (!object_matches.contains(type)) {
    Error(QStringLiteral("Json object is missing %1.").arg(type), object_matches);
    return;
  }
  const QJsonValue value_type = object_matches[type];

  if (!value_type.isArray()) {
    Error(u"Json album value in albummatches object is not an array."_s, value_type);
    return;
  }
  const QJsonArray array_type = value_type.toArray();

  for (const QJsonValue &value : array_type) {

    if (!value.isObject()) {
      Error(u"Invalid Json reply, value in albummatches/trackmatches array is not a object."_s);
      continue;
    }
    const QJsonObject object = value.toObject();
    if (!object.contains("artist"_L1) || !object.contains("image"_L1) || !object.contains("name"_L1)) {
      Error(u"Invalid Json reply, album is missing artist, image or name."_s, object);
      continue;
    }
    const QString artist = object["artist"_L1].toString();
    QString album;
    if (type == "album"_L1) {
      album = object["name"_L1].toString();
    }

    if (!object.contains("image"_L1) || !object["image"_L1].isArray()) {
      Error(u"Invalid Json reply, album image is not a array."_s, object);
      continue;
    }
    const QJsonArray array_image = object["image"_L1].toArray();
    QString image_url_use;
    LastFmImageSize image_size_use = LastFmImageSize::Unknown;
    for (const QJsonValue &value_image : array_image) {
      if (!value_image.isObject()) {
        Error(u"Invalid Json reply, album image value is not an object."_s);
        continue;
      }
      const QJsonObject object_image = value_image.toObject();
      if (!object_image.contains("#text"_L1) || !object_image.contains("size"_L1)) {
        Error(u"Invalid Json reply, album image value is missing #text or size."_s, object_image);
        continue;
      }
      const QString image_url = object_image["#text"_L1].toString();
      if (image_url.isEmpty()) continue;
      const LastFmImageSize image_size = ImageSizeFromString(object_image["size"_L1].toString().toLower());
      if (image_url_use.isEmpty() || image_size > image_size_use) {
        image_url_use = image_url;
        image_size_use = image_size;
      }
    }

    if (image_url_use.isEmpty()) continue;

    // Workaround for API limiting to 300x300 images.
    if (image_url_use.contains("/300x300/"_L1)) {
      image_url_use = image_url_use.replace("/300x300/"_L1, "/740x0/"_L1);
    }
    const QUrl url(image_url_use);
    if (!url.isValid()) continue;

    CoverProviderSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = url;
    cover_result.image_size = QSize(300, 300);
    results << cover_result;
  }

}

void LastFmCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Last.fm:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

LastFmCoverProvider::LastFmImageSize LastFmCoverProvider::ImageSizeFromString(const QString &size) {

  if (size == "small"_L1) return LastFmImageSize::Small;
  if (size == "medium"_L1) return LastFmImageSize::Medium;
  if (size == "large"_L1) return LastFmImageSize::Large;
  if (size == "extralarge"_L1) return LastFmImageSize::ExtraLarge;

  return LastFmImageSize::Unknown;

}
