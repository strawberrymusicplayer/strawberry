/*
 * Strawberry Music Player
 * Copyright 2020-2022, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QObject>
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
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "musixmatchlyricsprovider.h"
#include "providers/musixmatchprovider.h"

using std::make_shared;

MusixmatchLyricsProvider::MusixmatchLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider("Musixmatch", true, false, network, parent), use_api_(true) {}

MusixmatchLyricsProvider::~MusixmatchLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool MusixmatchLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  LyricsSearchContextPtr search = make_shared<LyricsSearchContext>();
  search->id = id;
  search->request = request;
  requests_search_.append(search);

  if (use_api_) {
    return SendSearchRequest(search);
  }
  else {
    return CreateLyricsRequest(search);
  }

}

void MusixmatchLyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

bool MusixmatchLyricsProvider::SendSearchRequest(LyricsSearchContextPtr search) {

  QUrlQuery url_query;
  url_query.addQueryItem("apikey", QByteArray::fromBase64(kApiKey));
  url_query.addQueryItem("q_artist", QUrl::toPercentEncoding(search->request.artist));
  url_query.addQueryItem("q_track", QUrl::toPercentEncoding(search->request.title));
  url_query.addQueryItem("f_has_lyrics", "1");

  QUrl url(QString(kApiUrl) + QString("/track.search"));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search]() { HandleSearchReply(reply, search); });

  qLog(Debug) << "MusixmatchLyrics: Sending request for" << url;

  return true;

}

void MusixmatchLyricsProvider::HandleSearchReply(QNetworkReply *reply, LyricsSearchContextPtr search) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() == 401 || reply->error() == 402) {
      Error(QString("Error %1 (%2) using API, switching to URL based lookup.").arg(reply->errorString()).arg(reply->error()));
      use_api_ = false;
      CreateLyricsRequest(search);
      return;
    }
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    EndSearch(search);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401 || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 402) {
      Error(QString("Received HTTP code %1 using API, switching to URL based lookup.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
      use_api_ = false;
      CreateLyricsRequest(search);
      return;
    }
    Error(QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    EndSearch(search);
    return;
  }

  QByteArray data = reply->readAll();
  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    EndSearch(search);
    return;
  }

  if (!json_obj.contains("message")) {
    Error("Json reply is missing message object.", json_obj);
    EndSearch(search);
    return;
  }
  if (!json_obj["message"].isObject()) {
    Error("Json reply message is not an object.", json_obj);
    EndSearch(search);
    return;
  }
  QJsonObject obj_message = json_obj["message"].toObject();

  if (!obj_message.contains("header")) {
    Error("Json reply message object is missing header.", obj_message);
    EndSearch(search);
    return;
  }
  if (!obj_message["header"].isObject()) {
    Error("Json reply message header is not an object.", obj_message);
    EndSearch(search);
    return;
  }
  QJsonObject obj_header = obj_message["header"].toObject();

  int status_code = obj_header["status_code"].toInt();
  if (status_code != 200) {
    Error(QString("Received status code %1, switching to URL based lookup.").arg(status_code));
    use_api_ = false;
    CreateLyricsRequest(search);
    return;
  }

  if (!obj_message.contains("body")) {
    Error("Json reply is missing body.", json_obj);
    EndSearch(search);
    return;
  }
  if (!obj_message["body"].isObject()) {
    Error("Json body is not an object.", json_obj);
    EndSearch(search);
    return;
  }
  QJsonObject obj_body = obj_message["body"].toObject();

  if (!obj_body.contains("track_list")) {
    Error("Json response is missing body.", obj_body);
    EndSearch(search);
    return;
  }
  if (!obj_body["track_list"].isArray()) {
    Error("Json hits is not an array.", obj_body);
    EndSearch(search);
    return;
  }
  QJsonArray array_tracklist = obj_body["track_list"].toArray();

  for (const QJsonValueRef value_track : array_tracklist) {
    if (!value_track.isObject()) {
      continue;
    }
    QJsonObject obj_track = value_track.toObject();

    if (!obj_track.contains("track") || !obj_track["track"].isObject()) {
      continue;
    }

    obj_track = obj_track["track"].toObject();
    if (!obj_track.contains("artist_name") ||
        !obj_track.contains("album_name") ||
        !obj_track.contains("track_name") ||
        !obj_track.contains("track_share_url")) {
      Error("Missing one or more values in result object", obj_track);
      continue;
    }

    QString artist_name = obj_track["artist_name"].toString();
    QString album_name = obj_track["album_name"].toString();
    QString track_name = obj_track["track_name"].toString();
    QUrl track_share_url(obj_track["track_share_url"].toString());

    // Ignore results where both the artist, album and title don't match.
    if (use_api_ &&
        artist_name.compare(search->request.albumartist, Qt::CaseInsensitive) != 0 &&
        artist_name.compare(search->request.artist, Qt::CaseInsensitive) != 0 &&
        album_name.compare(search->request.album, Qt::CaseInsensitive) != 0 &&
        track_name.compare(search->request.title, Qt::CaseInsensitive) != 0) {
      continue;
    }

    if (!track_share_url.isValid()) continue;

    if (search->requests_lyrics_.contains(track_share_url)) continue;
    search->requests_lyrics_.append(track_share_url);

  }

  if (search->requests_lyrics_.isEmpty()) {
    EndSearch(search);
  }
  else {
    for (const QUrl &url : search->requests_lyrics_) {
      SendLyricsRequest(search, url);
    }
  }

}

bool MusixmatchLyricsProvider::CreateLyricsRequest(LyricsSearchContextPtr search) {

  QString artist_stripped = StringFixup(search->request.artist);
  QString title_stripped = StringFixup(search->request.title);
  if (artist_stripped.isEmpty() || title_stripped.isEmpty()) {
    EndSearch(search);
    return false;
  }

  QUrl url(QString("https://www.musixmatch.com/lyrics/%1/%2").arg(artist_stripped, title_stripped));
  search->requests_lyrics_.append(url);
  return SendLyricsRequest(search, url);

}

bool MusixmatchLyricsProvider::SendLyricsRequest(LyricsSearchContextPtr search, const QUrl &url) {

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search, url]() { HandleLyricsReply(reply, search, url); });

  qLog(Debug) << "MusixmatchLyrics: Sending request for" << url;

  return true;

}

void MusixmatchLyricsProvider::HandleLyricsReply(QNetworkReply *reply, LyricsSearchContextPtr search, const QUrl &url) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    EndSearch(search, url);
    return;
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    EndSearch(search, url);
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error("Empty reply received from server.");
    EndSearch(search, url);
    return;
  }

  QString content = data;
  QString data_begin = "<script id=\"__NEXT_DATA__\" type=\"application/json\">";
  QString data_end = "</script>";
  qint64 begin_idx = content.indexOf(data_begin);
  QString content_json;
  if (begin_idx > 0) {
    begin_idx += data_begin.length();
    qint64 end_idx = content.indexOf(data_end, begin_idx);
    if (end_idx > begin_idx) {
      content_json = content.mid(begin_idx, end_idx - begin_idx);
    }
  }

  if (content_json.isEmpty()) {
    EndSearch(search, url);
    return;
  }

  if (content_json.contains(QRegularExpression("<[^>]*>"))) {  // Make sure it's not HTML code.
    EndSearch(search, url);
    return;
  }

  QJsonObject obj_data = ExtractJsonObj(content_json.toUtf8());
  if (obj_data.isEmpty()) {
    EndSearch(search, url);
    return;
  }

  if (!obj_data.contains("props") || !obj_data["props"].isObject()) {
    Error("Json reply is missing props.", obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data["props"].toObject();

  if (!obj_data.contains("pageProps") || !obj_data["pageProps"].isObject()) {
    Error("Json props is missing pageProps.", obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data["pageProps"].toObject();

  if (!obj_data.contains("data") || !obj_data["data"].isObject()) {
    Error("Json pageProps is missing data.", obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data["data"].toObject();


  if (!obj_data.contains("trackInfo") || !obj_data["trackInfo"].isObject()) {
    Error("Json data is missing trackInfo.", obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data["trackInfo"].toObject();

  if (!obj_data.contains("data") || !obj_data["data"].isObject()) {
    Error("Json trackInfo reply is missing data.", obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data["data"].toObject();

  if (!obj_data.contains("track") || !obj_data["track"].isObject()) {
    Error("Json data is missing track.", obj_data);
    EndSearch(search, url);
    return;
  }

  const QJsonObject obj_track = obj_data["track"].toObject();

  if (!obj_track.contains("hasLyrics") || !obj_track["hasLyrics"].isBool()) {
    Error("Json track is missing hasLyrics.", obj_track);
    EndSearch(search, url);
    return;
  }

  const bool has_lyrics = obj_track["hasLyrics"].toBool();
  if (!has_lyrics) {
    EndSearch(search, url);
    return;
  }

  LyricsSearchResult result;
  if (obj_track.contains("artistName") && obj_track["artistName"].isString()) {
    result.artist = obj_track["artistName"].toString();
  }
  if (obj_track.contains("albumName") && obj_track["albumName"].isString()) {
    result.album = obj_track["albumName"].toString();
  }
  if (obj_track.contains("name") && obj_track["name"].isString()) {
    result.title = obj_track["name"].toString();
  }

  if (!obj_data.contains("lyrics") || !obj_data["lyrics"].isObject()) {
    Error("Json data is missing lyrics.", obj_data);
    EndSearch(search, url);
    return;
  }
  QJsonObject obj_lyrics = obj_data["lyrics"].toObject();

  if (!obj_lyrics.contains("body") || !obj_lyrics["body"].isString()) {
    Error("Json lyrics reply is missing body.", obj_lyrics);
    EndSearch(search, url);
    return;
  }
  result.lyrics = obj_lyrics["body"].toString();

  if (!result.lyrics.isEmpty()) {
    result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);
    search->results.append(result);
  }

  EndSearch(search, url);

}

void MusixmatchLyricsProvider::EndSearch(LyricsSearchContextPtr search, const QUrl &url) {

  if (search->requests_lyrics_.contains(url)) {
    search->requests_lyrics_.removeAll(url);
  }

  if (search->requests_lyrics_.count() == 0) {
    requests_search_.removeAll(search);
    if (search->results.isEmpty()) {
      qLog(Debug) << "MusixmatchLyrics: No lyrics for" << search->request.artist << search->request.title;
    }
    else {
      qLog(Debug) << "MusixmatchLyrics: Got lyrics for" << search->request.artist << search->request.title;
    }
    emit SearchFinished(search->id, search->results);
  }

}

void MusixmatchLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "MusixmatchLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
