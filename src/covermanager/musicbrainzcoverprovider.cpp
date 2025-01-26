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

#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "musicbrainzcoverprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kReleaseSearchUrl[] = "https://musicbrainz.org/ws/2/release/";
constexpr char kAlbumCoverUrl[] = "https://coverartarchive.org/release/%1/front";
constexpr int kLimit = 8;
constexpr int kRequestsDelay = 1000;
}  // namespace

MusicbrainzCoverProvider::MusicbrainzCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"MusicBrainz"_s, true, false, 1.5, true, false, network, parent),
      timer_flush_requests_(new QTimer(this)) {

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &MusicbrainzCoverProvider::FlushRequests);

}

bool MusicbrainzCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  Q_UNUSED(title);

  if (artist.isEmpty() || album.isEmpty()) return false;

  SearchRequest request(id, artist, album);
  queue_search_requests_ << request;

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

  return true;

}

void MusicbrainzCoverProvider::SendSearchRequest(const SearchRequest &request) {

  const QString query = QStringLiteral("release:\"%1\" AND artist:\"%2\"").arg(request.album.trimmed().replace(u'"', "\""_L1), request.artist.trimmed().replace(u'"', "\""_L1));
  QUrlQuery url_query;
  url_query.addQueryItem(u"query"_s, query);
  url_query.addQueryItem(u"limit"_s, QString::number(kLimit));
  url_query.addQueryItem(u"fmt"_s, u"json"_s);
  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(kReleaseSearchUrl)), url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { HandleSearchReply(reply, request.id); });

}

void MusicbrainzCoverProvider::FlushRequests() {

  if (!queue_search_requests_.isEmpty()) {
    SendSearchRequest(queue_search_requests_.dequeue());
    return;
  }

  timer_flush_requests_->stop();

}

JsonBaseRequest::JsonObjectResult MusicbrainzCoverProvider::ParseJsonObject(QNetworkReply *reply) {

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
      if (json_object.contains("error"_L1) && json_object.contains("help"_L1)) {
        const QString error = json_object["error"_L1].toString();
        const QString help = json_object["help"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(error, help);
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

void MusicbrainzCoverProvider::HandleSearchReply(QNetworkReply *reply, const int search_id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverProviderSearchResults results;
  const QScopeGuard search_finished = qScopeGuard([this, search_id, &results]() { Q_EMIT SearchFinished(search_id, results); });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("releases"_L1)) {
    Error(u"Json reply is missing releases."_s, json_object);
    return;
  }

  const QJsonValue value_releases = json_object["releases"_L1];

  if (!value_releases.isArray()) {
    Error(u"Json releases is not an array."_s, value_releases);
    return;
  }
  const QJsonArray array_releases = value_releases.toArray();

  if (array_releases.isEmpty()) {
    return;
  }

  for (const QJsonValue &value_release : array_releases) {

    if (!value_release.isObject()) {
      Error(u"Invalid Json reply, releases array value is not an object."_s);
      continue;
    }
    const QJsonObject object_release = value_release.toObject();
    if (!object_release.contains("id"_L1) || !object_release.contains("artist-credit"_L1) || !object_release.contains("title"_L1)) {
      Error(u"Invalid Json reply, releases array object is missing id, artist-credit or title."_s, object_release);
      continue;
    }

    const QJsonValue value_artists = object_release["artist-credit"_L1];
    if (!value_artists.isArray()) {
      Error(u"Invalid Json reply, artist-credit is not a array."_s, value_artists);
      continue;
    }
    const QJsonArray array_artists = value_artists.toArray();
    int i = 0;
    QString artist;
    for (const QJsonValue &value_artist : array_artists) {
      if (!value_artist.isObject()) {
        Error(u"Invalid Json reply, artist is not a object."_s);
        continue;
      }
      const QJsonObject object_artist = value_artist.toObject();

      if (!object_artist.contains("artist"_L1)) {
        Error(u"Invalid Json reply, artist is missing."_s, object_artist);
        continue;
      }
      const QJsonValue value_artist2 = object_artist["artist"_L1];
      if (!value_artist2.isObject()) {
        Error(u"Invalid Json reply, artist is not an object."_s, value_artist2);
        continue;
      }
      const QJsonObject obj_artist2 = value_artist2.toObject();

      if (!obj_artist2.contains("name"_L1)) {
        Error(u"Invalid Json reply, artist is missing name."_s, value_artist2);
        continue;
      }
      artist = obj_artist2["name"_L1].toString();
      ++i;
    }
    if (i > 1) artist = "Various artists"_L1;

    const QString id = object_release["id"_L1].toString();
    const QString album = object_release["title"_L1].toString();

    CoverProviderSearchResult cover_result;
    const QUrl url(QString::fromLatin1(kAlbumCoverUrl).arg(id));
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = url;
    results.append(cover_result);
  }

}

void MusicbrainzCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Musicbrainz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
