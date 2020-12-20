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
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QtDebug>

#include "core/logging.h"
#include "core/network.h"
#include "jsonlyricsprovider.h"
#include "lyricsfetcher.h"
#include "lyricsprovider.h"
#include "musixmatchlyricsprovider.h"

MusixmatchLyricsProvider::MusixmatchLyricsProvider(QObject *parent) : JsonLyricsProvider("Musixmatch", true, false, parent), network_(new NetworkAccessManager(this)) {}

MusixmatchLyricsProvider::~MusixmatchLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool MusixmatchLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const quint64 id) {

  QString artist_stripped = artist;
  QString title_stripped = title;

  artist_stripped = artist_stripped.replace('/', '-');
  artist_stripped = artist_stripped.remove(QRegularExpression("[^\\w0-9\\- ]", QRegularExpression::UseUnicodePropertiesOption));
  artist_stripped = artist_stripped.simplified();
  artist_stripped = artist_stripped.replace(' ', '-');
  artist_stripped = artist_stripped.replace(QRegularExpression("(-)\\1+"), "-");
  artist_stripped = artist_stripped.toLower();

  title_stripped = title_stripped.replace('/', '-');
  title_stripped = title_stripped.remove(QRegularExpression("[^\\w0-9\\- ]", QRegularExpression::UseUnicodePropertiesOption));
  title_stripped = title_stripped.simplified();
  title_stripped = title_stripped.replace(' ', '-').toLower();
  title_stripped = title_stripped.replace(QRegularExpression("(-)\\1+"), "-");
  title_stripped = title_stripped.toLower();

  if (artist_stripped.isEmpty() || title_stripped.isEmpty()) return false;

  QUrl url(QString("https://www.musixmatch.com/lyrics/%1/%2").arg(artist_stripped).arg(title_stripped));
  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
#endif
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, id, artist, album, title); });

  qLog(Debug) << "MusixmatchLyrics: Sending request for" << artist_stripped << title_stripped << url;

  return true;

}

void MusixmatchLyricsProvider::CancelSearch(const quint64 id) { Q_UNUSED(id); }

void MusixmatchLyricsProvider::HandleSearchReply(QNetworkReply *reply, const quint64 id, const QString &artist, const QString &album, const QString &title) {

  Q_UNUSED(album);

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  LyricsSearchResults results;

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    emit SearchFinished(id, results);
    return;
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    emit SearchFinished(id, results);
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error("Empty reply received from server.");
    emit SearchFinished(id, results);
    return;
  }

  QString content = data;
  QString data_begin = "var __mxmState = ";
  QString data_end = ";</script>";
  int begin_idx = content.indexOf(data_begin);
  QString content_json;
  if (begin_idx > 0) {
    begin_idx += data_begin.length();
    int end_idx = content.indexOf(data_end, begin_idx);
    if (end_idx > begin_idx) {
      content_json = content.mid(begin_idx, end_idx - begin_idx);
    }
  }

  if (content_json.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  if (content_json.contains(QRegularExpression("<[^>]*>"))) { // Make sure it's not HTML code.
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(content_json.toUtf8());
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  if (!json_obj.contains("page") || !json_obj["page"].isObject()) {
    Error("Json reply is missing page.", json_obj);
    emit SearchFinished(id, results);
    return;
  }
  json_obj = json_obj["page"].toObject();


  if (!json_obj.contains("track") || !json_obj["track"].isObject()) {
    Error("Json reply is missing track.", json_obj);
    emit SearchFinished(id, results);
    return;
  }
  QJsonObject obj_track = json_obj["track"].toObject();

  if (!obj_track.contains("artistName") || !obj_track.contains("albumName") || !obj_track.contains("name")) {
    Error("Json track is missing artistName, albumName or name.", json_obj);
    emit SearchFinished(id, results);
    return;
  }

  if (!json_obj.contains("lyrics") || !json_obj["lyrics"].isObject()) {
    Error("Json reply is missing lyrics.", json_obj);
    emit SearchFinished(id, results);
    return;
  }
  QJsonObject obj_lyrics = json_obj["lyrics"].toObject();

  if (!obj_lyrics.contains("lyrics") || !obj_lyrics["lyrics"].isObject()) {
    Error("Json reply is missing lyrics.", obj_lyrics);
    emit SearchFinished(id, results);
    return;
  }
  obj_lyrics = obj_lyrics["lyrics"].toObject();

  if (!obj_lyrics.contains("body")) {
    Error("Json lyrics is missing body.", obj_lyrics);
    emit SearchFinished(id, results);
  }

  LyricsSearchResult result;
  result.artist = obj_track["artistName"].toString();
  result.album = obj_track["albumName"].toString();
  result.title = obj_track["name"].toString();
  result.lyrics = obj_lyrics["body"].toString();

  if (!result.lyrics.isEmpty() && (artist.toLower() == result.artist.toLower() || title.toLower() == result.title.toLower())) {
    results.append(result);
  }

  if (results.isEmpty()) {
    qLog(Debug) << "MusixmatchLyrics: No lyrics for" << artist << title;
  }
  else {
    qLog(Debug) << "MusixmatchLyrics: Got lyrics for" << artist << title;
  }

  emit SearchFinished(id, results);

}

void MusixmatchLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "MusixmatchLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
