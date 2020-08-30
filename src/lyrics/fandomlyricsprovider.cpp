/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/logging.h"
#include "core/network.h"
#include "jsonlyricsprovider.h"
#include "lyricsfetcher.h"
#include "lyricsprovider.h"
#include "fandomlyricsprovider.h"

const char *FandomLyricsProvider::kApiUrl = "https://lyrics.fandom.com/api.php";

FandomLyricsProvider::FandomLyricsProvider(QObject *parent) : JsonLyricsProvider("Fandom", true, false, parent), network_(new NetworkAccessManager(this)) {}

FandomLyricsProvider::~FandomLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool FandomLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const quint64 id) {

  Q_UNUSED(album);

  QString query = artist + QLatin1Char(':') + title;
  query = query.replace(' ', '_');

  const ParamList params = ParamList() << Param("action", "query")
                                       << Param("prop", "revisions")
                                       << Param("rvprop", "content")
                                       << Param("format", "json")
                                       << Param("titles", query);

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kApiUrl);
  url.setQuery(url_query);
  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, id, artist, title); });

  //qLog(Debug) << "FandomLyrics: Sending request for" << url;

  return true;

}

void FandomLyricsProvider::CancelSearch(const quint64 id) { Q_UNUSED(id); }

void FandomLyricsProvider::HandleSearchReply(QNetworkReply *reply, const quint64 id, const QString &artist, const QString &title) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QJsonArray json_result = ExtractResult(reply, artist, title);
  if (json_result.isEmpty()) {
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }

  LyricsSearchResults results;
  for (const QJsonValue &value : json_result) {
    if (!value.isObject()) continue;
    QJsonObject json_obj = value.toObject();
    if (json_obj.keys().count() == 0) continue;
    QString content = json_obj[json_obj.keys().first()].toString();
    QString data_begin = "<lyrics>";
    QString data_end = "</lyrics>";
    int begin_idx = content.indexOf(data_begin);
    QString lyrics;
    if (begin_idx > 0) {
      begin_idx += data_begin.length();
      int end_idx = content.indexOf(data_end, begin_idx);
      if (end_idx > begin_idx) {
        lyrics = content.mid(begin_idx, end_idx - begin_idx);
     }
    }
    if (lyrics.isEmpty()) continue;
    LyricsSearchResult result;
    result.lyrics = lyrics;
    results << result;
  }

  if (results.isEmpty()) qLog(Debug) << "FandomLyrics: No lyrics for" << artist << title;
  else qLog(Debug) << "FandomLyrics: Got lyrics for" << artist << title;

  emit SearchFinished(id, results);

}

QJsonArray FandomLyricsProvider::ExtractResult(QNetworkReply *reply, const QString &artist, const QString &title) {

  QJsonObject json_obj = ExtractJsonObj(reply);
  if (json_obj.isEmpty()) return QJsonArray();

  if (!json_obj.contains("query")) {
    Error("JSON reply is missing query object.", json_obj);
    return QJsonArray();
  }
  if (!json_obj["query"].isObject()) {
    Error("Failed to parse JSON: query in not an object.", json_obj);
    return QJsonArray();
  }
  json_obj = json_obj["query"].toObject();

  if (!json_obj.contains("pages")) {
    Error("JSON query object is missing pages object.", json_obj);
    return QJsonArray();
  }
  if (!json_obj["pages"].isObject()) {
    Error("JSON pages in query is not an object.", json_obj);
    return QJsonArray();
  }
  json_obj = json_obj["pages"].toObject();

  if (json_obj.keys().count() == 0) {
    Error("JSON pages is missing object.", json_obj);
    return QJsonArray();
  }
  json_obj = json_obj[json_obj.keys().first()].toObject();

  if (json_obj.contains("missing")) {
    return QJsonArray();
  }

  if (!json_obj.contains("revisions")) {
    Error("JSON reply is missing revisions.", json_obj);
    return QJsonArray();
  }

  if (!json_obj["revisions"].isArray()) {
    Error("JSON revisions is not an array.", json_obj);
    return QJsonArray();
  }

  QJsonArray json_result = json_obj["revisions"].toArray();
  if (json_result.isEmpty()) {
    Error(QString("No lyrics for %1 %2").arg(artist).arg(title));
    return QJsonArray();
  }

  return json_result;

}

void FandomLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "FandomLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
