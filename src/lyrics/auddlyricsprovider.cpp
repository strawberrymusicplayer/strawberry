/*
 * Strawberry Music Player
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "auddlyricsprovider.h"

const char *AuddLyricsProvider::kUrlSearch = "https://api.audd.io/findLyrics/";
const int AuddLyricsProvider::kMaxLength = 6000;

AuddLyricsProvider::AuddLyricsProvider(NetworkAccessManager *network, QObject *parent) : JsonLyricsProvider("AudD", false, false, network, parent) {}

AuddLyricsProvider::~AuddLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool AuddLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  QUrl url(kUrlSearch);
  QUrlQuery url_query;
  url_query.addQueryItem("q", QUrl::toPercentEncoding(QString(request.artist + " " + request.title)));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

  return true;

}

void AuddLyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void AuddLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const QByteArray data = ExtractData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(id);
    return;
  }

  QJsonArray json_result = ExtractResult(data);
  if (json_result.isEmpty()) {
    qLog(Debug) << "AudDLyrics: No lyrics for" << request.artist << request.title;
    emit SearchFinished(id);
    return;
  }

  LyricsSearchResults results;
  for (const QJsonValueRef value : json_result) {
    if (!value.isObject()) {
      qLog(Error) << "AudDLyrics: Invalid Json reply, result is not an object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj = value.toObject();
    if (
       !json_obj.contains("song_id") ||
       !json_obj.contains("artist_id") ||
       !json_obj.contains("title") ||
       !json_obj.contains("artist") ||
       !json_obj.contains("lyrics")
       ) {
      qLog(Error) << "AudDLyrics: Invalid Json reply, result is missing data.";
      qLog(Debug) << value;
      continue;
    }
    LyricsSearchResult result;
    result.artist = json_obj["artist"].toString();
    result.title = json_obj["title"].toString();
    if (result.artist.compare(request.albumartist, Qt::CaseInsensitive) != 0 &&
        result.artist.compare(request.artist, Qt::CaseInsensitive) != 0 &&
        result.title.compare(request.title, Qt::CaseInsensitive) != 0) {
      continue;
    }
    result.lyrics = json_obj["lyrics"].toString();
    if (result.lyrics.isEmpty() || result.lyrics.length() > kMaxLength || result.lyrics == "error") continue;
    result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);
    results << result;
  }

  if (results.isEmpty()) {
    qLog(Debug) << "AudDLyrics: No lyrics for" << request.artist << request.title;
  }
  else {
    qLog(Debug) << "AudDLyrics: Got lyrics for" << request.artist << request.title;
  }

  emit SearchFinished(id, results);

}

QJsonArray AuddLyricsProvider::ExtractResult(const QByteArray &data) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonArray();

  if (!json_obj.contains("status")) {
    Error("Json reply is missing status.", json_obj);
    return QJsonArray();
  }

  if (json_obj["status"].toString() == "error") {
    if (!json_obj.contains("error")) {
      Error("Json reply is missing error status.", json_obj);
      return QJsonArray();
    }
    QJsonObject json_error = json_obj["error"].toObject();
    if (!json_error.contains("error_code") || !json_error.contains("error_message")) {
      Error("Json reply is missing error code or message.", json_error);
      return QJsonArray();
    }
    QString error_message = json_error["error_message"].toString();
    Error(error_message);
    return QJsonArray();
  }

  if (!json_obj.contains("result") || !json_obj["result"].isArray()) {
    Error("Json reply is missing result array.", json_obj);
    return QJsonArray();
  }

  return json_obj["result"].toArray();

}

void AuddLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "AudDLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
