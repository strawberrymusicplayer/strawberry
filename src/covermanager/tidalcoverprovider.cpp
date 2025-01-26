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

#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/song.h"
#include "tidal/tidalservice.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "tidalcoverprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kLimit = 10;
}  // namespace

TidalCoverProvider::TidalCoverProvider(const TidalServicePtr service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Tidal"_s, true, true, 2.5, true, true, network, parent),
      service_(service) {}

bool TidalCoverProvider::authenticated() const {

  return service_->authenticated();

}

bool TidalCoverProvider::use_authorization_header() const {

  return true;

}

QByteArray TidalCoverProvider::authorization_header() const {

  return service_->authorization_header();

}

void TidalCoverProvider::ClearSession() {

  service_->ClearSession();

}

bool TidalCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (!service_ || !service_->authenticated()) return false;

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString resource;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    resource = "search/tracks"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    resource = "search/albums"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  const ParamList params = ParamList() << Param(u"query"_s, query)
                                       << Param(u"limit"_s, QString::number(kLimit))
                                       << Param(u"countryCode"_s, service_->country_code());

  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(TidalService::kApiUrl) + QLatin1Char('/') + resource), params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { HandleSearchReply(reply, id); });

  return true;

}

void TidalCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

JsonBaseRequest::JsonObjectResult TidalCoverProvider::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  bool clear_session = false;
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains("status"_L1) && json_object.contains("subStatus"_L1) && json_object.contains("userMessage"_L1)) {
        const int status = json_object["status"_L1].toInt();
        const int sub_status = json_object["subStatus"_L1].toInt();
        const QString user_message = json_object["userMessage"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.api_error = status;
        result.error_message = QStringLiteral("%1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
        if (status == 401 && sub_status == 6001) {
          clear_session = true;
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

  if (reply->error() == QNetworkReply::AuthenticationRequiredError || clear_session) {
    service_->ClearSession();
  }

  return result;

}

void TidalCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

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

  const QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("items"_L1)) {
    Error(u"Json object is missing items."_s, json_object);
    return;
  }
  const QJsonValue value_items = json_object["items"_L1];

  if (!value_items.isArray()) {
    return;
  }
  const QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {
    return;
  }

  int i = 0;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      Error(u"Invalid Json reply, items array item is not a object."_s);
      continue;
    }
    const QJsonObject object_item = value_item.toObject();

    if (!object_item.contains("artist"_L1)) {
      Error(u"Invalid Json reply, items array item is missing artist."_s, object_item);
      continue;
    }
    const QJsonValue value_artist = object_item["artist"_L1];
    if (!value_artist.isObject()) {
      Error(u"Invalid Json reply, items array item artist is not a object."_s, value_artist);
      continue;
    }
    const QJsonObject object_artist = value_artist.toObject();
    if (!object_artist.contains("name"_L1)) {
      Error(u"Invalid Json reply, items array item artist is missing name."_s, object_artist);
      continue;
    }
    const QString artist = object_artist["name"_L1].toString();

    QJsonObject object_album;
    if (object_item.contains("album"_L1)) {
      QJsonValue value_album = object_item["album"_L1];
      if (value_album.isObject()) {
        object_album = value_album.toObject();
      }
      else {
        Error(u"Invalid Json reply, items array item album is not a object."_s, value_album);
        continue;
      }
    }
    else {
      object_album = object_item;
    }

    if (!object_album.contains("title"_L1) || !object_album.contains("cover"_L1)) {
      Error(u"Invalid Json reply, items array item album is missing title or cover."_s, object_album);
      continue;
    }
    const QString album = object_album["title"_L1].toString();
    const QString cover = object_album["cover"_L1].toString().replace("-"_L1, "/"_L1);

    CoverProviderSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = Song::AlbumRemoveDiscMisc(album);
    cover_result.number = ++i;

    const QList<QPair<QString, QSize>> cover_sizes = QList<QPair<QString, QSize>>() << qMakePair(u"1280x1280"_s, QSize(1280, 1280))
                                                                                    << qMakePair(u"750x750"_s, QSize(750, 750))
                                                                                    << qMakePair(u"640x640"_s, QSize(640, 640));
    for (const QPair<QString, QSize> &cover_size : cover_sizes) {
      QUrl cover_url(QStringLiteral("%1/images/%2/%3.jpg").arg(QLatin1String(TidalService::kResourcesUrl), cover, cover_size.first));
      cover_result.image_url = cover_url;
      cover_result.image_size = cover_size.second;
      results << cover_result;
    }

  }

}

void TidalCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
