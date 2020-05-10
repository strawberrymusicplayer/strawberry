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
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTextCodec>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QtDebug>

#include "core/logging.h"
#include "core/network.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "musixmatchcoverprovider.h"

MusixmatchCoverProvider::MusixmatchCoverProvider(Application *app, QObject *parent): JsonCoverProvider("Musixmatch", true, false, 1.0, true, false, app, parent), network_(new NetworkAccessManager(this)) {}

bool MusixmatchCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  Q_UNUSED(title);

  QString artist_stripped = artist;
  QString album_stripped = album;

  artist_stripped = artist_stripped.replace('/', '-');
  artist_stripped = artist_stripped.remove(QRegExp("[^A-Za-z0-9\\- ]"));
  artist_stripped = artist_stripped.simplified();
  artist_stripped = artist_stripped.replace(' ', '-');
  artist_stripped = artist_stripped.replace(QRegExp("(-)\\1+"), "-");
  artist_stripped = artist_stripped.toLower();

  album_stripped = album_stripped.replace('/', '-');
  album_stripped = album_stripped.remove(QRegExp("[^a-zA-Z0-9\\- ]"));
  album_stripped = album_stripped.simplified();
  album_stripped = album_stripped.replace(' ', '-').toLower();
  album_stripped = album_stripped.replace(QRegExp("(-)\\1+"), "-");
  album_stripped = album_stripped.toLower();

  if (artist_stripped.isEmpty() || album_stripped.isEmpty()) return false;

  QUrl url(QString("https://www.musixmatch.com/album/%1/%2").arg(artist_stripped).arg(album_stripped));
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  QNetworkReply *reply = network_->get(req);
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, id, artist, album); });

  //qLog(Debug) << "Musixmatch: Sending request for" << artist_stripped << album_stripped << url;

  return true;

}

void MusixmatchCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void MusixmatchCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &artist, const QString &album) {

  reply->deleteLater();

  CoverSearchResults results;

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

  QTextCodec *codec = QTextCodec::codecForName("utf-8");
  if (!codec) {
    emit SearchFinished(id, results);
    return;
  }
  QString content = codec->toUnicode(data);

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

  if (content_json.contains(QRegExp("<[^>]*>"))) { // Make sure it's not HTML code.
    emit SearchFinished(id, results);
    return;
  }

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(content_json.toUtf8(), &error);

  if (error.error != QJsonParseError::NoError) {
    Error(QString("Failed to parse json data: %1").arg(error.errorString()));
    emit SearchFinished(id, results);
    return;
  }

  if (json_doc.isEmpty()) {
    Error("Received empty Json document.", data);
    emit SearchFinished(id, results);
    return;
  }

  if (!json_doc.isObject()) {
    Error("Json document is not an object.", json_doc);
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error("Received empty Json object.", json_doc);
    emit SearchFinished(id, results);
    return;
  }

  if (!json_obj.contains("page") || !json_obj["page"].isObject()) {
    Error("Json reply is missing page object.", json_obj);
    emit SearchFinished(id, results);
    return;
  }
  json_obj = json_obj["page"].toObject();

  if (!json_obj.contains("album") || !json_obj["album"].isObject()) {
    Error("Json page object is missing album object.", json_obj);
    emit SearchFinished(id, results);
    return;
  }
  QJsonObject obj_album = json_obj["album"].toObject();

  if (!obj_album.contains("artistName") || !obj_album.contains("name")) {
    Error("Json album object is missing artistName or name.", obj_album);
    emit SearchFinished(id, results);
    return;
  }

  QString cover;

  if (obj_album.contains("coverart800x800")) {
    cover = obj_album["coverart800x800"].toString();
  }
  else if (obj_album.contains("coverart500x500")) {
    cover = obj_album["coverart500x500"].toString();
  }
  else if (obj_album.contains("coverart350x350")) {
    cover = obj_album["coverart350x350"].toString();
  }

  if (cover.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }
  QUrl cover_url(cover);
  if (!cover_url.isValid()) {
    Error("Received cover url is not valid.", cover);
    emit SearchFinished(id, results);
    return;
  }

  CoverSearchResult result;
  result.artist = obj_album["artistName"].toString();
  result.album = obj_album["name"].toString();
  result.image_url = cover_url;

  if (artist.toLower() == result.artist.toLower() || album.toLower() == result.album.toLower()) {
    results.append(result);
  }

  emit SearchFinished(id, results);

}

void MusixmatchCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Musixmatch:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
