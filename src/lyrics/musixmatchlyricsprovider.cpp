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

MusixmatchLyricsProvider::MusixmatchLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(QStringLiteral("Musixmatch"), true, false, network, parent), use_api_(true) {}

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
  url_query.addQueryItem(QStringLiteral("apikey"), QString::fromLatin1(QByteArray::fromBase64(kApiKey)));
  url_query.addQueryItem(QStringLiteral("q_artist"), QString::fromLatin1(QUrl::toPercentEncoding(search->request.artist)));
  url_query.addQueryItem(QStringLiteral("q_track"), QString::fromLatin1(QUrl::toPercentEncoding(search->request.title)));
  url_query.addQueryItem(QStringLiteral("f_has_lyrics"), QStringLiteral("1"));

  QUrl url(QString::fromLatin1(kApiUrl) + QStringLiteral("/track.search"));
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
      Error(QStringLiteral("Error %1 (%2) using API, switching to URL based lookup.").arg(reply->errorString()).arg(reply->error()));
      use_api_ = false;
      CreateLyricsRequest(search);
      return;
    }
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    EndSearch(search);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401 || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 402) {
      Error(QStringLiteral("Received HTTP code %1 using API, switching to URL based lookup.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
      use_api_ = false;
      CreateLyricsRequest(search);
      return;
    }
    Error(QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    EndSearch(search);
    return;
  }

  QByteArray data = reply->readAll();
  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    EndSearch(search);
    return;
  }

  if (!json_obj.contains(QLatin1String("message"))) {
    Error(QStringLiteral("Json reply is missing message object."), json_obj);
    EndSearch(search);
    return;
  }
  if (!json_obj[QLatin1String("message")].isObject()) {
    Error(QStringLiteral("Json reply message is not an object."), json_obj);
    EndSearch(search);
    return;
  }
  QJsonObject obj_message = json_obj[QLatin1String("message")].toObject();

  if (!obj_message.contains(QLatin1String("header"))) {
    Error(QStringLiteral("Json reply message object is missing header."), obj_message);
    EndSearch(search);
    return;
  }
  if (!obj_message[QLatin1String("header")].isObject()) {
    Error(QStringLiteral("Json reply message header is not an object."), obj_message);
    EndSearch(search);
    return;
  }
  QJsonObject obj_header = obj_message[QLatin1String("header")].toObject();

  int status_code = obj_header[QLatin1String("status_code")].toInt();
  if (status_code != 200) {
    Error(QStringLiteral("Received status code %1, switching to URL based lookup.").arg(status_code));
    use_api_ = false;
    CreateLyricsRequest(search);
    return;
  }

  if (!obj_message.contains(QLatin1String("body"))) {
    Error(QStringLiteral("Json reply is missing body."), json_obj);
    EndSearch(search);
    return;
  }
  if (!obj_message[QLatin1String("body")].isObject()) {
    Error(QStringLiteral("Json body is not an object."), json_obj);
    EndSearch(search);
    return;
  }
  QJsonObject obj_body = obj_message[QLatin1String("body")].toObject();

  if (!obj_body.contains(QLatin1String("track_list"))) {
    Error(QStringLiteral("Json response is missing body."), obj_body);
    EndSearch(search);
    return;
  }
  if (!obj_body[QLatin1String("track_list")].isArray()) {
    Error(QStringLiteral("Json hits is not an array."), obj_body);
    EndSearch(search);
    return;
  }
  QJsonArray array_tracklist = obj_body[QLatin1String("track_list")].toArray();

  for (const QJsonValueRef value_track : array_tracklist) {
    if (!value_track.isObject()) {
      continue;
    }
    QJsonObject obj_track = value_track.toObject();

    if (!obj_track.contains(QLatin1String("track")) || !obj_track[QLatin1String("track")].isObject()) {
      continue;
    }

    obj_track = obj_track[QLatin1String("track")].toObject();
    if (!obj_track.contains(QLatin1String("artist_name")) ||
        !obj_track.contains(QLatin1String("album_name")) ||
        !obj_track.contains(QLatin1String("track_name")) ||
        !obj_track.contains(QLatin1String("track_share_url"))) {
      Error(QStringLiteral("Missing one or more values in result object"), obj_track);
      continue;
    }

    QString artist_name = obj_track[QLatin1String("artist_name")].toString();
    QString album_name = obj_track[QLatin1String("album_name")].toString();
    QString track_name = obj_track[QLatin1String("track_name")].toString();
    QUrl track_share_url(obj_track[QLatin1String("track_share_url")].toString());

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

  QUrl url(QStringLiteral("https://www.musixmatch.com/lyrics/%1/%2").arg(artist_stripped, title_stripped));
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
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    EndSearch(search, url);
    return;
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    EndSearch(search, url);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Empty reply received from server."));
    EndSearch(search, url);
    return;
  }

  const QString content = QString::fromUtf8(data);
  const QString data_begin = QLatin1String("<script id=\"__NEXT_DATA__\" type=\"application/json\">");
  const QString data_end = QLatin1String("</script>");
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

  if (content_json.contains(QRegularExpression(QStringLiteral("<[^>]*>")))) {  // Make sure it's not HTML code.
    EndSearch(search, url);
    return;
  }

  QJsonObject obj_data = ExtractJsonObj(content_json.toUtf8());
  if (obj_data.isEmpty()) {
    EndSearch(search, url);
    return;
  }

  if (!obj_data.contains(QLatin1String("props")) || !obj_data[QLatin1String("props")].isObject()) {
    Error(QStringLiteral("Json reply is missing props."), obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data[QLatin1String("props")].toObject();

  if (!obj_data.contains(QLatin1String("pageProps")) || !obj_data[QLatin1String("pageProps")].isObject()) {
    Error(QStringLiteral("Json props is missing pageProps."), obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data[QLatin1String("pageProps")].toObject();

  if (!obj_data.contains(QLatin1String("data")) || !obj_data[QLatin1String("data")].isObject()) {
    Error(QStringLiteral("Json pageProps is missing data."), obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data[QLatin1String("data")].toObject();


  if (!obj_data.contains(QLatin1String("trackInfo")) || !obj_data[QLatin1String("trackInfo")].isObject()) {
    Error(QStringLiteral("Json data is missing trackInfo."), obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data[QLatin1String("trackInfo")].toObject();

  if (!obj_data.contains(QLatin1String("data")) || !obj_data[QLatin1String("data")].isObject()) {
    Error(QStringLiteral("Json trackInfo reply is missing data."), obj_data);
    EndSearch(search, url);
    return;
  }
  obj_data = obj_data[QLatin1String("data")].toObject();

  if (!obj_data.contains(QLatin1String("track")) || !obj_data[QLatin1String("track")].isObject()) {
    Error(QStringLiteral("Json data is missing track."), obj_data);
    EndSearch(search, url);
    return;
  }

  const QJsonObject obj_track = obj_data[QLatin1String("track")].toObject();

  if (!obj_track.contains(QLatin1String("hasLyrics")) || !obj_track[QLatin1String("hasLyrics")].isBool()) {
    Error(QStringLiteral("Json track is missing hasLyrics."), obj_track);
    EndSearch(search, url);
    return;
  }

  const bool has_lyrics = obj_track[QLatin1String("hasLyrics")].toBool();
  if (!has_lyrics) {
    EndSearch(search, url);
    return;
  }

  LyricsSearchResult result;
  if (obj_track.contains(QLatin1String("artistName")) && obj_track[QLatin1String("artistName")].isString()) {
    result.artist = obj_track[QLatin1String("artistName")].toString();
  }
  if (obj_track.contains(QLatin1String("albumName")) && obj_track[QLatin1String("albumName")].isString()) {
    result.album = obj_track[QLatin1String("albumName")].toString();
  }
  if (obj_track.contains(QLatin1String("name")) && obj_track[QLatin1String("name")].isString()) {
    result.title = obj_track[QLatin1String("name")].toString();
  }

  if (!obj_data.contains(QLatin1String("lyrics")) || !obj_data[QLatin1String("lyrics")].isObject()) {
    Error(QStringLiteral("Json data is missing lyrics."), obj_data);
    EndSearch(search, url);
    return;
  }
  QJsonObject obj_lyrics = obj_data[QLatin1String("lyrics")].toObject();

  if (!obj_lyrics.contains(QLatin1String("body")) || !obj_lyrics[QLatin1String("body")].isString()) {
    Error(QStringLiteral("Json lyrics reply is missing body."), obj_lyrics);
    EndSearch(search, url);
    return;
  }
  result.lyrics = obj_lyrics[QLatin1String("body")].toString();

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
