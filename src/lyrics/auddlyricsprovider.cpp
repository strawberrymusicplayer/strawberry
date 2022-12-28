/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QPair>
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
#include "jsonlyricsprovider.h"
#include "lyricsfetcher.h"
#include "lyricsprovider.h"
#include "auddlyricsprovider.h"

const char *AuddLyricsProvider::kUrlSearch = "https://api.audd.io/findLyrics/";
const char *AuddLyricsProvider::kAPITokenB64 = "ZjA0NjQ4YjgyNDM3ZTc1MjY3YjJlZDI5ZDBlMzQxZjk=";
const int AuddLyricsProvider::kMaxLength = 6000;

AuddLyricsProvider::AuddLyricsProvider(NetworkAccessManager *network, QObject *parent) : JsonLyricsProvider("AudD", true, false, network, parent) {}

AuddLyricsProvider::~AuddLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool AuddLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  Q_UNUSED(album);

  const ParamList params = ParamList() << Param("api_token", QByteArray::fromBase64(kAPITokenB64))
                                       << Param("q", QString(artist + " " + title));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kUrlSearch);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, artist, title]() { HandleSearchReply(reply, id, artist, title); });

  //qLog(Debug) << "AudDLyrics: Sending request for" << url;

  return true;

}

void AuddLyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void AuddLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &artist, const QString &title) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QJsonArray json_result = ExtractResult(reply, artist, title);
  if (json_result.isEmpty()) {
    emit SearchFinished(id, LyricsSearchResults());
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
    if (result.artist.compare(artist, Qt::CaseInsensitive) != 0 && result.title.compare(title, Qt::CaseInsensitive) != 0) continue;
    result.lyrics = json_obj["lyrics"].toString();
    if (result.lyrics.isEmpty() || result.lyrics.length() > kMaxLength || result.lyrics == "error") continue;
    result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);

    //qLog(Debug) << "AudDLyrics:" << result.artist << result.title << result.lyrics.length();

    results << result;
  }

  if (results.isEmpty()) qLog(Debug) << "AudDLyrics: No lyrics for" << artist << title;
  else qLog(Debug) << "AudDLyrics: Got lyrics for" << artist << title;

  emit SearchFinished(id, results);

}

QJsonArray AuddLyricsProvider::ExtractResult(QNetworkReply *reply, const QString &artist, const QString &title) {

  QJsonObject json_obj = ExtractJsonObj(reply);
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

  if (!json_obj.contains("result")) {
    Error("Json reply is missing result.", json_obj);
    return QJsonArray();
  }

  QJsonArray json_result = json_obj["result"].toArray();
  if (json_result.isEmpty()) {
    Error(QString("No lyrics for %1 %2").arg(artist, title));
    return QJsonArray();
  }

  return json_result;

}

void AuddLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "AudDLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
