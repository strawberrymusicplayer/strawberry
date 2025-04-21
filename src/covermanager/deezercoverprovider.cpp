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

#include <QPair>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QScopeGuard>

#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/song.h"
#include "albumcoverfetcher.h"
#include "albumcoverfetchersearch.h"
#include "jsoncoverprovider.h"
#include "deezercoverprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kApiUrl[] = "https://api.deezer.com";
constexpr int kLimit = 10;
}  // namespace

DeezerCoverProvider::DeezerCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Deezer"_s, true, false, 2.0, true, true, network, parent) {}

bool DeezerCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString resource;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    resource = "search/track"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    resource = "search/album"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  const ParamList params = ParamList() << Param(u"output"_s, u"json"_s)
                                       << Param(u"q"_s, query)
                                       << Param(u"limit"_s, QString::number(kLimit));

  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(kApiUrl) + QLatin1Char('/') + resource), params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { HandleSearchReply(reply, id); });

  return true;

}

void DeezerCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

JsonBaseRequest::JsonObjectResult DeezerCoverProvider::ParseJsonObject(QNetworkReply *reply) {

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
        if (object_error.contains("code"_L1) && object_error.contains("type"_L1) && object_error.contains("message"_L1)) {
          const int code = object_error["code"_L1].toInt();
          const QString type = object_error["type"_L1].toString();
          const QString message = object_error["message"_L1].toString();
          result.error_code = ErrorCode::APIError;
          result.error_message = QStringLiteral("%1: %2 (%3)").arg(type, message).arg(code);
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

  return result;

}

void DeezerCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

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

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  QJsonArray array_data;
  if (json_object.contains("data"_L1) && json_object["data"_L1].isArray()) {
    array_data = json_object["data"_L1].toArray();
  }
  else if (json_object.contains("DATA"_L1) && json_object["DATA"_L1].isArray()) {
    array_data = json_object["DATA"_L1].toArray();
  }
  else {
    Error(u"Json reply object is missing data."_s, json_object);
    return;
  }

  if (array_data.isEmpty()) {
    return;
  }

  QMap<QUrl, CoverProviderSearchResult> cover_results;
  int i = 0;
  for (const QJsonValue &value_entry : std::as_const(array_data)) {

    if (!value_entry.isObject()) {
      Error(u"Invalid Json reply, data array value is not a object."_s);
      continue;
    }
    const QJsonObject object_entry = value_entry.toObject();
    QJsonObject object_album;
    if (object_entry.contains("album"_L1) && object_entry["album"_L1].isObject()) {  // Song search, so extract the album.
      object_album = object_entry["album"_L1].toObject();
    }
    else {
      object_album = object_entry;
    }

    if (!object_entry.contains("id"_L1) || !object_album.contains("id"_L1)) {
      Error(u"Invalid Json reply, data array value object is missing ID."_s, object_entry);
      continue;
    }

    if (!object_album.contains("type"_L1)) {
      Error(u"Invalid Json reply, data array value album object is missing type."_s, object_album);
      continue;
    }
    const QString type = object_album["type"_L1].toString();
    if (type != "album"_L1) {
      Error(u"Invalid Json reply, data array value album object has incorrect type returned"_s, object_album);
      continue;
    }

    if (!object_entry.contains("artist"_L1)) {
      Error(u"Invalid Json reply, data array value object is missing artist."_s, object_entry);
      continue;
    }
    const QJsonValue value_artist = object_entry["artist"_L1];
    if (!value_artist.isObject()) {
      Error(u"Invalid Json reply, data array value artist is not a object."_s, value_artist);
      continue;
    }
    const QJsonObject object_artist = value_artist.toObject();

    if (!object_artist.contains("name"_L1)) {
      Error(u"Invalid Json reply, data array value artist object is missing name."_s, object_artist);
      continue;
    }
    const QString artist = object_artist["name"_L1].toString();

    if (!object_album.contains("title"_L1)) {
      Error(u"Invalid Json reply, data array value album object is missing title."_s, object_album);
      continue;
    }
    const QString album = object_album["title"_L1].toString();

    CoverProviderSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = Song::AlbumRemoveDiscMisc(album);

    bool have_cover = false;
    const QList<QPair<QString, QSize>> cover_sizes = QList<QPair<QString, QSize>>() << qMakePair(u"cover_xl"_s, QSize(1000, 1000))
                                                                                    << qMakePair(u"cover_big"_s, QSize(500, 500));
    for (const QPair<QString, QSize> &cover_size : cover_sizes) {
      if (!object_album.contains(cover_size.first)) continue;
      const QString cover = object_album[cover_size.first].toString();
      if (!have_cover) {
        have_cover = true;
        ++i;
      }
      const QUrl url(cover);
      if (!cover_results.contains(url)) {
        cover_result.image_url = url;
        cover_result.image_size = cover_size.second;
        cover_result.number = i;
        cover_results.insert(url, cover_result);
      }
    }

    if (!have_cover) {
      Error(u"Invalid Json reply, data array value album object is missing cover."_s, object_album);
    }

  }

  results = cover_results.values();
  std::stable_sort(results.begin(), results.end(), AlbumCoverFetcherSearch::CoverProviderSearchResultCompareNumber);

}

void DeezerCoverProvider::Error(const QString &error, const QVariant &debug) {
  qLog(Error) << "Deezer:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
}
