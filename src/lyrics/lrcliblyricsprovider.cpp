/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "lrcliblyricsprovider.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {
constexpr char kApiUrl[] = "https://lrclib.net/api/get";
}  // namespace

LrcLibLyricsProvider::LrcLibLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(u"LrcLib"_s, true, false, network, parent) {}

void LrcLibLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  const QUrl url(QString::fromUtf8(kApiUrl));
  QUrlQuery url_query;
  url_query.addQueryItem(u"track_name"_s, QString::fromLatin1(QUrl::toPercentEncoding(request.title)));
  url_query.addQueryItem(u"artist_name"_s, QString::fromLatin1(QUrl::toPercentEncoding(request.artist)));
  url_query.addQueryItem(u"album_name"_s, QString::fromLatin1(QUrl::toPercentEncoding(request.album)));
  url_query.addQueryItem(u"duration"_s, QString::number(request.duration));
  QNetworkReply *reply = CreateGetRequest(url, url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

  qLog(Debug) << "LrcLibLyrics: Sending request for" << url << url_query.toString();

}

LrcLibLyricsProvider::JsonObjectResult LrcLibLyricsProvider::ParseJsonObject(QNetworkReply *reply) {

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
      if (json_object.contains("statusCode"_L1) && json_object.contains("name"_L1) && json_object.contains("message"_L1)) {
        const int code = json_object["statusCode"_L1].toInt();
        const QString name = json_object["name"_L1].toString();
        const QString message = json_object["message"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2) (%3)").arg(code).arg(name, message);
        result.api_error = code;
      }
      else {
        result.json_object = json_object;
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

void LrcLibLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  LyricsSearchResults results;
  const QScopeGuard end_search = qScopeGuard([this, id, request, &results]() { EndSearch(id, request, results); });

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    if (json_object_result.api_error != 404) {
      Error(json_object_result.error_message);
    }
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty() ||
      !json_object.contains("trackName"_L1) ||
      !json_object.contains("artistName"_L1) ||
      !json_object.contains("albumName"_L1) ||
      !json_object.contains("plainLyrics"_L1)) {
    return;
  }

  LyricsSearchResult result;
  result.artist = json_object["artistName"_L1].toString();
  result.album = json_object["albumName"_L1].toString();
  result.title = json_object["trackName"_L1].toString();
  result.lyrics = json_object["plainLyrics"_L1].toString();
  results << result;

}

void LrcLibLyricsProvider::EndSearch(const int id, const LyricsSearchRequest &request, const LyricsSearchResults &results) {

  if (results.isEmpty()) {
    qLog(Debug) << name_ << "No lyrics for" << request.artist << request.album << request.title;
  }
  else {
    qLog(Debug) << name_ << "Got lyrics for" << request.artist << request.album << request.title;
  }

  Q_EMIT SearchFinished(id, results);

}
