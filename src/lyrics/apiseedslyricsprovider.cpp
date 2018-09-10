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

#include <QtGlobal>
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
#include "apiseedslyricsprovider.h"

const char *APISeedsLyricsProvider::kUrlSearch = "https://orion.apiseeds.com/api/music/lyric";
const char *APISeedsLyricsProvider::kAPIKeyB64 = "REdWenJhR245Qm03cnE5NlhoS1pTd0V5UVNCNjBtTWVEZlp0ZEttVXhKZTRRdnZSbTRYcmlaUVlaMlM3c0JQUw==";

APISeedsLyricsProvider::APISeedsLyricsProvider(QObject *parent) : LyricsProvider("APISeeds", parent), network_(new NetworkAccessManager(this)) {}

bool APISeedsLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, quint64 id) {

  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;
  typedef QPair<QByteArray, QByteArray> EncodedArg;

  ArgList args = ArgList();
  args.append(Arg("apikey", QByteArray::fromBase64(kAPIKeyB64)));

  QUrlQuery url_query;
  for (const Arg &arg : args) {
    EncodedArg encoded_arg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
    url_query.addQueryItem(encoded_arg.first, encoded_arg.second);
  }

  QUrl url(QString("%1/%2/%3").arg(kUrlSearch).arg(QString::fromLatin1(QUrl::toPercentEncoding(artist))).arg(QString::fromLatin1(QUrl::toPercentEncoding(title))));
  url.setQuery(url_query);
  QNetworkReply *reply = network_->get(QNetworkRequest(url));
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply*, quint64, QString, QString)), reply, id, artist, title);

  //qLog(Debug) << "APISeedsLyrics: Sending request for" << url;

  return true;

}

void APISeedsLyricsProvider::CancelSearch(quint64 id) {
}

void APISeedsLyricsProvider::HandleSearchReply(QNetworkReply *reply, quint64 id, const QString artist, const QString title) {

  reply->deleteLater();

  QJsonObject json_obj = ExtractResult(reply, id);
  if (json_obj.isEmpty()) return;

  if (!json_obj.contains("artist") || !json_obj.contains("track")) {
    Error(id, "APISeedsLyrics: Invalid Json reply, result is missing artist or track.", json_obj);
    return;
  }
  QJsonObject json_artist(json_obj["artist"].toObject());
  QJsonObject json_track(json_obj["track"].toObject());
  if (!json_track.contains("text")) {
    Error(id, "APISeedsLyrics: Invalid Json reply, track is missing text.", json_obj);
    return;
  }

  LyricsSearchResults results;
  LyricsSearchResult result;
  result.artist = json_artist["name"].toString();
  result.title = json_track["name"].toString();
  result.lyrics = json_track["text"].toString();
  result.score = 0.0;
  if (result.artist.toLower() == artist.toLower()) result.score += 1.0;
  if (result.title.toLower() == title.toLower()) result.score += 1.0;

  //qLog(Debug) << "APISeedsLyrics:" << result.artist << result.title << result.lyrics;

  results << result;

  emit SearchFinished(id, results);

}

QJsonObject APISeedsLyricsProvider::ExtractJsonObj(QNetworkReply *reply, quint64 id) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError) {
    data = reply->readAll();
  }
  else {
    QString failure_reason;
    if (reply->error() < 200) {
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else {
      // See if there is JSON data containing "error" - then use that instead.
      data = reply->readAll();
      QJsonParseError error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);
      if (error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error")) {
          failure_reason = json_obj["error"].toString();
          // Don't bother showing error when there was no match.
          if (failure_reason == "Lyric no found, try again later.") failure_reason.clear();
        }
        else {
          failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
      }
      else {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
    }
    Error(id, failure_reason);
    return QJsonObject();
  }

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

QJsonObject APISeedsLyricsProvider::ExtractResult(QNetworkReply *reply, quint64 id) {

  QJsonObject json_obj = ExtractJsonObj(reply, id);
  if (json_obj.isEmpty()) return QJsonObject();

  if (json_obj.contains("error")) {
    Error(id, json_obj["error"].toString(), json_obj);
    return QJsonObject();
  }

  if (!json_obj.contains("result")) {
    Error(id, "Json reply is missing result.", json_obj);
    return QJsonObject();
  }

  QJsonObject json_result = json_obj["result"].toObject();
  if (json_result.isEmpty()) {
    Error(id, "Json result object is empty.");
    return QJsonObject();
  }
  return json_result;

}

void APISeedsLyricsProvider::Error(quint64 id, QString error, QVariant debug) {
  LyricsSearchResults results;
  if (!error.isEmpty()) qLog(Error) << "APISeedsLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
  emit SearchFinished(id, results);
}
