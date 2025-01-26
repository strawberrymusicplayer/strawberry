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

#include <utility>
#include <memory>

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

#include "includes/shared_ptr.h"
#include "utilities/musixmatchprovider.h"
#include "utilities/strutils.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "musixmatchlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;
using namespace MusixmatchProvider;

namespace {
constexpr char kApiUrl[] = "https://api.musixmatch.com/ws/1.1";
constexpr char kApiKey[] = "Y2FhMDRlN2Y4OWE5OTIxYmZlOGMzOWQzOGI3ZGU4MjE=";
}  // namespace

MusixmatchLyricsProvider::MusixmatchLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(u"Musixmatch"_s, true, false, network, parent), use_api_(true) {}

void MusixmatchLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  LyricsSearchContextPtr search = make_shared<LyricsSearchContext>();
  search->id = id;
  search->request = request;
  requests_search_.append(search);

  if (use_api_) {
    SendSearchRequest(search);
    return;
  }

  CreateLyricsRequest(search);

}

bool MusixmatchLyricsProvider::SendSearchRequest(LyricsSearchContextPtr search) {

  const QUrl url(QLatin1String(kApiUrl) + "/track.search"_L1);
  QUrlQuery url_query;
  url_query.addQueryItem(u"apikey"_s, QString::fromLatin1(QByteArray::fromBase64(kApiKey)));
  url_query.addQueryItem(u"q_artist"_s, QString::fromLatin1(QUrl::toPercentEncoding(search->request.artist)));
  url_query.addQueryItem(u"q_track"_s, QString::fromLatin1(QUrl::toPercentEncoding(search->request.title)));
  url_query.addQueryItem(u"f_has_lyrics"_s, u"1"_s);
  QNetworkReply *reply = CreateGetRequest(url, url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search]() { HandleSearchReply(reply, search); });

  qLog(Debug) << "MusixmatchLyrics: Sending request for" << url;

  return true;

}

void MusixmatchLyricsProvider::HandleSearchReply(QNetworkReply *reply, LyricsSearchContextPtr search) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  const QScopeGuard end_search = qScopeGuard([this, search]() { EndSearch(search); });

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
    return;
  }

  const QByteArray data = reply->readAll();
  const QJsonObject json_obj = GetJsonObject(data).json_object;
  if (json_obj.isEmpty()) {
    return;
  }

  if (!json_obj.contains("message"_L1)) {
    Error(u"Json reply is missing message object."_s, json_obj);
    return;
  }
  if (!json_obj["message"_L1].isObject()) {
    Error(u"Json reply message is not an object."_s, json_obj);
    return;
  }
  QJsonObject obj_message = json_obj["message"_L1].toObject();

  if (!obj_message.contains("header"_L1)) {
    Error(u"Json reply message object is missing header."_s, obj_message);
    return;
  }
  if (!obj_message["header"_L1].isObject()) {
    Error(u"Json reply message header is not an object."_s, obj_message);
    return;
  }
  QJsonObject obj_header = obj_message["header"_L1].toObject();

  int status_code = obj_header["status_code"_L1].toInt();
  if (status_code != 200) {
    Error(QStringLiteral("Received status code %1, switching to URL based lookup.").arg(status_code));
    use_api_ = false;
    CreateLyricsRequest(search);
    return;
  }

  if (!obj_message.contains("body"_L1)) {
    Error(u"Json reply is missing body."_s, json_obj);
    return;
  }
  if (!obj_message["body"_L1].isObject()) {
    Error(u"Json body is not an object."_s, json_obj);
    return;
  }
  QJsonObject obj_body = obj_message["body"_L1].toObject();

  if (!obj_body.contains("track_list"_L1)) {
    Error(u"Json response is missing body."_s, obj_body);
    return;
  }
  if (!obj_body["track_list"_L1].isArray()) {
    Error(u"Json hits is not an array."_s, obj_body);
    return;
  }
  const QJsonArray array_tracklist = obj_body["track_list"_L1].toArray();

  for (const QJsonValue &value_track : array_tracklist) {
    if (!value_track.isObject()) {
      continue;
    }
    QJsonObject object_track = value_track.toObject();

    if (!object_track.contains("track"_L1) || !object_track["track"_L1].isObject()) {
      continue;
    }

    object_track = object_track["track"_L1].toObject();
    if (!object_track.contains("artist_name"_L1) ||
        !object_track.contains("album_name"_L1) ||
        !object_track.contains("track_name"_L1) ||
        !object_track.contains("track_share_url"_L1)) {
      Error(u"Missing one or more values in result object"_s, object_track);
      continue;
    }

    const QString artist_name = object_track["artist_name"_L1].toString();
    const QString album_name = object_track["album_name"_L1].toString();
    const QString track_name = object_track["track_name"_L1].toString();
    const QUrl track_share_url(object_track["track_share_url"_L1].toString());

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

  for (const QUrl &url : std::as_const(search->requests_lyrics_)) {
    SendLyricsRequest(search, url);
  }

}

bool MusixmatchLyricsProvider::CreateLyricsRequest(LyricsSearchContextPtr search) {

  const QString artist_stripped = StringFixup(search->request.artist);
  const QString title_stripped = StringFixup(search->request.title);
  if (artist_stripped.isEmpty() || title_stripped.isEmpty()) {
    EndSearch(search);
    return false;
  }

  const QUrl url(QStringLiteral("https://www.musixmatch.com/lyrics/%1/%2").arg(artist_stripped, title_stripped));
  search->requests_lyrics_.append(url);
  return SendLyricsRequest(search, url);

}

bool MusixmatchLyricsProvider::SendLyricsRequest(LyricsSearchContextPtr search, const QUrl &url) {

  qLog(Debug) << "MusixmatchLyrics: Sending request for" << url;

  QNetworkReply *reply = CreateGetRequest(url);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search, url]() { HandleLyricsReply(reply, search, url); });

  return true;

}

void MusixmatchLyricsProvider::HandleLyricsReply(QNetworkReply *reply, LyricsSearchContextPtr search, const QUrl &url) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  const QScopeGuard end_search = qScopeGuard([this, search, url]() { EndSearch(search, url); });

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    return;
  }
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(u"Empty reply received from server."_s);
    return;
  }

  const QString content = QString::fromUtf8(data);
  const QString data_begin = "<script id=\"__NEXT_DATA__\" type=\"application/json\">"_L1;
  const QString data_end = "</script>"_L1;
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
    return;
  }

  static const QRegularExpression regex_html_tag(u"<[^>]*>"_s);
  if (content_json.contains(regex_html_tag)) {  // Make sure it's not HTML code.
    return;
  }

  QJsonObject object_data = GetJsonObject(content_json.toUtf8()).json_object;
  if (object_data.isEmpty()) {
    return;
  }

  if (!object_data.contains("props"_L1) || !object_data["props"_L1].isObject()) {
    Error(u"Json reply is missing props."_s, object_data);
    return;
  }
  object_data = object_data["props"_L1].toObject();

  if (!object_data.contains("pageProps"_L1) || !object_data["pageProps"_L1].isObject()) {
    Error(u"Json props is missing pageProps."_s, object_data);
    return;
  }
  object_data = object_data["pageProps"_L1].toObject();

  if (!object_data.contains("data"_L1) || !object_data["data"_L1].isObject()) {
    Error(u"Json pageProps is missing data."_s, object_data);
    return;
  }
  object_data = object_data["data"_L1].toObject();


  if (!object_data.contains("trackInfo"_L1) || !object_data["trackInfo"_L1].isObject()) {
    Error(u"Json data is missing trackInfo."_s, object_data);
    return;
  }
  object_data = object_data["trackInfo"_L1].toObject();

  if (!object_data.contains("data"_L1) || !object_data["data"_L1].isObject()) {
    Error(u"Json trackInfo reply is missing data."_s, object_data);
    return;
  }
  object_data = object_data["data"_L1].toObject();

  if (!object_data.contains("track"_L1) || !object_data["track"_L1].isObject()) {
    Error(u"Json data is missing track."_s, object_data);
    return;
  }

  const QJsonObject obj_track = object_data["track"_L1].toObject();

  if (!obj_track.contains("hasLyrics"_L1) || !obj_track["hasLyrics"_L1].isBool()) {
    Error(u"Json track is missing hasLyrics."_s, obj_track);
    return;
  }

  const bool has_lyrics = obj_track["hasLyrics"_L1].toBool();
  if (!has_lyrics) {
    return;
  }

  LyricsSearchResult result;
  if (obj_track.contains("artistName"_L1) && obj_track["artistName"_L1].isString()) {
    result.artist = obj_track["artistName"_L1].toString();
  }
  if (obj_track.contains("albumName"_L1) && obj_track["albumName"_L1].isString()) {
    result.album = obj_track["albumName"_L1].toString();
  }
  if (obj_track.contains("name"_L1) && obj_track["name"_L1].isString()) {
    result.title = obj_track["name"_L1].toString();
  }

  if (!object_data.contains("lyrics"_L1) || !object_data["lyrics"_L1].isObject()) {
    Error(u"Json data is missing lyrics."_s, object_data);
    return;
  }
  QJsonObject object_lyrics = object_data["lyrics"_L1].toObject();

  if (!object_lyrics.contains("body"_L1) || !object_lyrics["body"_L1].isString()) {
    Error(u"Json lyrics reply is missing body."_s, object_lyrics);
    return;
  }
  result.lyrics = object_lyrics["body"_L1].toString();

  if (!result.lyrics.isEmpty()) {
    result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);
    search->results.append(result);
  }

}

void MusixmatchLyricsProvider::EndSearch(LyricsSearchContextPtr search, const QUrl &url) {

  if (!url.isEmpty() && search->requests_lyrics_.contains(url)) {
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
    Q_EMIT SearchFinished(search->id, search->results);
  }

}
