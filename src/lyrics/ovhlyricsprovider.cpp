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

#include <QApplication>
#include <QThread>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "jsonlyricsprovider.h"
#include "ovhlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrlSearch[] = "https://api.lyrics.ovh/v1/";
}  // namespace

OVHLyricsProvider::OVHLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(u"Lyrics.ovh"_s, true, false, network, parent) {}

void OVHLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  const QUrl url(QLatin1String(kUrlSearch) + QString::fromLatin1(QUrl::toPercentEncoding(request.artist)) + '/'_L1 + QString::fromLatin1(QUrl::toPercentEncoding(request.title)));
  QNetworkReply *reply = CreateGetRequest(url);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

}

OVHLyricsProvider::JsonObjectResult OVHLyricsProvider::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return JsonObjectResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
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
      if (json_object.contains("error"_L1)) {
        result.error_code = ErrorCode::APIError;
        result.error_message = json_object["error"_L1].toString();
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

void OVHLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  LyricsSearchResults results;
  const QScopeGuard search_finished = qScopeGuard([this, id, &results]() { Q_EMIT SearchFinished(id, results); });

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_reply = ParseJsonObject(reply);
  if (!json_object_reply.success()) {
    qLog(Debug) << "OVHLyrics" << json_object_reply.error_message;
    return;
  }

  const QJsonObject &json_object = json_object_reply.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (json_object.contains("error"_L1)) {
    Error(json_object["error"_L1].toString());
    qLog(Debug) << "OVHLyrics: No lyrics for" << request.artist << request.title;
    return;
  }

  if (!json_object.contains("lyrics"_L1)) {
    return;
  }

  const QString lyrics = json_object["lyrics"_L1].toString();

  if (lyrics.isEmpty()) {
    qLog(Debug) << "OVHLyrics: No lyrics for" << request.artist << request.title;
  }
  else {
    qLog(Debug) << "OVHLyrics: Got lyrics for" << request.artist << request.title;
    results << LyricsSearchResult(Utilities::DecodeHtmlEntities(lyrics));
 }

}
