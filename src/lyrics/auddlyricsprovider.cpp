/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QList>
#include <QPair>
#include <QMap>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/utilities.h"
#include "lyricsprovider.h"
#include "lyricsfetcher.h"
#include "auddlyricsprovider.h"

const char *AuddLyricsProvider::kUrlSearch = "https://api.audd.io/findLyrics/";
const char *AuddLyricsProvider::kAPITokenB64 = "ZjA0NjQ4YjgyNDM3ZTc1MjY3YjJlZDI5ZDBlMzQxZjk=";
const int AuddLyricsProvider::kMaxLength = 6000;

AuddLyricsProvider::AuddLyricsProvider(QObject *parent) : LyricsProvider("AudD", parent), network_(new NetworkAccessManager(this)) {}

bool AuddLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const quint64 id) {

  const ParamList params = ParamList() << Param("api_token", QByteArray::fromBase64(kAPITokenB64))
                                       << Param("q", QString(artist + " " + title));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kUrlSearch);
  url.setQuery(url_query);
  QNetworkReply *reply = network_->get(QNetworkRequest(url));
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply*, const quint64, const QString&, const QString&)), reply, id, artist, title);

  //qLog(Debug) << "AudDLyrics: Sending request for" << url;

  return true;

}

void AuddLyricsProvider::CancelSearch(quint64 id) {
}

void AuddLyricsProvider::HandleSearchReply(QNetworkReply *reply, const quint64 id, const QString &artist, const QString &title) {

  reply->deleteLater();

  QJsonArray json_result = ExtractResult(reply, id, artist, title);
  if (json_result.isEmpty()) {
    return;
  }

  LyricsSearchResults results;
  for (const QJsonValue &value : json_result) {
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
    if (result.artist.toLower() != artist.toLower() && result.title.toLower() != title.toLower()) continue;
    result.lyrics = json_obj["lyrics"].toString();
    if (result.lyrics.length() > kMaxLength) continue;
    if (result.lyrics == "error") continue;
    result.score = 0.0;
    if (result.artist.toLower() == artist.toLower()) result.score += 1.0;
    if (result.title.toLower() == title.toLower()) result.score += 1.0;
    //qLog(Debug) << "AudDLyrics:" << result.artist << result.title << result.lyrics.length();

    results << result;
  }

  if (results.isEmpty()) qLog(Debug) << "AudDLyrics: No lyrics for" << artist << title;
  else qLog(Debug) << "AudDLyrics: Got lyrics for" << artist << title;

  emit SearchFinished(id, results);

}

QJsonObject AuddLyricsProvider::ExtractJsonObj(QNetworkReply *reply, const quint64 id) {

  if (reply->error() != QNetworkReply::NoError) {
    QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    Error(id, failure_reason);
    return QJsonObject();
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    QString failure_reason = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    Error(id, failure_reason);
    return QJsonObject();
  }

  QByteArray data(reply->readAll());

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    Error(id, "Reply from server missing Json data.");
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error(id, "Received empty Json document.");
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error(id, "Json document is not an object.");
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error(id, "Received empty Json object.");
    return QJsonObject();
  }

  return json_obj;

}

QJsonArray AuddLyricsProvider::ExtractResult(QNetworkReply *reply, const quint64 id, const QString &artist, const QString &title) {

  QJsonObject json_obj = ExtractJsonObj(reply, id);
  if (json_obj.isEmpty()) return QJsonArray();

  if (!json_obj.contains("status")) {
    Error(id, "Json reply is missing status.", json_obj);
    return QJsonArray();
  }

  if (json_obj["status"].toString() == "error") {
    if (!json_obj.contains("error")) {
      Error(id, "Json reply is missing error status.", json_obj);
      return QJsonArray();
    }
    QJsonObject json_error = json_obj["error"].toObject();
    if (!json_error.contains("error_code") || !json_error.contains("error_message")) {
      Error(id, "Json reply is missing error code or message.", json_error);
      return QJsonArray();
    }
    QString error_code(json_error["error_code"].toString());
    QString error_message(json_error["error_message"].toString());
    Error(id, error_message);
    return QJsonArray();
  }

  if (!json_obj.contains("result")) {
    Error(id, "Json reply is missing result.", json_obj);
    return QJsonArray();
  }

  QJsonArray json_result = json_obj["result"].toArray();
  if (json_result.isEmpty()) {
    Error(id, QString("No lyrics for %1 %2").arg(artist).arg(title));
    return QJsonArray();
  }

  return json_result;

}

void AuddLyricsProvider::Error(const quint64 id, const QString &error, QVariant debug) {
  qLog(Error) << "AudDLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
  emit SearchFinished(id, LyricsSearchResults());
}
