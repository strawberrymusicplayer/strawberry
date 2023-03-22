/*
 * Strawberry Music Player
 * Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "lyricscomlyricsprovider.h"

const char *LyricsComLyricsProvider::kApiUrl = "https://www.abbreviations.com/services/v2/lyrics.php";
const char *LyricsComLyricsProvider::kLyricsUrl = "https://www.lyrics.com/lyrics/";
const char *LyricsComLyricsProvider::kUID = "11363";
const char *LyricsComLyricsProvider::kTokenB64 = "b3FOYmxhV1ZKRGxIMnV4OA==";

LyricsComLyricsProvider::LyricsComLyricsProvider(NetworkAccessManager *network, QObject *parent) : JsonLyricsProvider("Lyrics.com", true, false, network, parent), use_api_(true) {}

LyricsComLyricsProvider::~LyricsComLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool LyricsComLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  if (use_api_) {
    SendSearchRequest(id, request);
  }
  else {
    CreateLyricsRequest(id, request);
  }

  return true;

}

void LyricsComLyricsProvider::SendSearchRequest(const int id, const LyricsSearchRequest &request) {

  QUrlQuery url_query;
  url_query.addQueryItem(QUrl::toPercentEncoding("uid"), QUrl::toPercentEncoding(kUID));
  url_query.addQueryItem(QUrl::toPercentEncoding("tokenid"), QUrl::toPercentEncoding(QByteArray::fromBase64(kTokenB64)));
  url_query.addQueryItem(QUrl::toPercentEncoding("format"), "json");
  url_query.addQueryItem(QUrl::toPercentEncoding("artist"), QUrl::toPercentEncoding(request.artist));
  url_query.addQueryItem(QUrl::toPercentEncoding("album"), QUrl::toPercentEncoding(request.album));
  url_query.addQueryItem(QUrl::toPercentEncoding("term"), QUrl::toPercentEncoding(request.title));
  QUrl url(kApiUrl);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

}

void LyricsComLyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void LyricsComLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = ExtractData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(id);
    return;
  }

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    qLog(Error) << "Lyrics.com: Failed to parse json data" << json_error.errorString();
    emit SearchFinished(id);
    return;
  }

  if (json_doc.isEmpty()) {
    qLog(Debug) << "Lyrics.com: No lyrics for" << request.artist << request.album << request.title;
    emit SearchFinished(id);
    return;
  }

  if (!json_doc.isObject()) {
    qLog(Error) << "Lyrics.com: Json document is not an object.";
    emit SearchFinished(id);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    qLog(Debug) << "Lyrics.com: No lyrics for" << request.artist << request.album << request.title;
    emit SearchFinished(id);
    return;
  }

  if (json_obj.contains("error")) {
    const QString error = json_obj["error"].toString();
    qLog(Error) << "Lyrics.com: Received error:" << error << "switching to URL based lookup.";
    use_api_ = false;
    CreateLyricsRequest(id, request);
    return;
  }

  if (!json_obj.contains("result") || !json_obj["result"].isArray()) {
    qLog(Error) << "Lyrics.com: Json reply is missing result.";
    emit SearchFinished(id);
    return;
  }

  QJsonArray json_result = json_obj["result"].toArray();
  if (json_result.isEmpty()) {
    qLog(Debug) << "Lyrics.com: No lyrics for" << request.artist << request.album << request.title;
    emit SearchFinished(id);
    return;
  }

  for (const QJsonValueRef value : json_result) {
    if (!value.isObject()) {
      qLog(Error) << "Lyrics.com: Invalid Json reply, result is not an object.";
      continue;
    }
    QJsonObject obj = value.toObject();
    if (
      !obj.contains("song") ||
      !obj.contains("artist") ||
      !obj.contains("album") ||
      !obj.contains("song-link")
    ) {
      qLog(Error) << "Lyrics.com: Invalid Json reply, result is missing data.";
      qLog(Debug) << value;
      continue;
    }
    QString result_artist = obj["artist"].toString();
    QString result_album = obj["album"].toString();
    QString result_title = obj["song"].toString();
    QString song_link = obj["song-link"].toString();

    if (result_artist.compare(request.albumartist, Qt::CaseInsensitive) != 0 &&
        result_artist.compare(request.artist, Qt::CaseInsensitive) != 0 &&
        result_album.compare(request.album, Qt::CaseInsensitive) != 0 &&
        result_title.compare(request.title, Qt::CaseInsensitive) != 0) {
      continue;
    }

    if (!song_link.isEmpty() && QRegularExpression("^https:\\/\\/.*\\/lyric\\/\\d+\\/.*\\/.*$").match(song_link).hasMatch()) {
      song_link = song_link.replace(QRegularExpression("\\/lyric\\/\\d+\\/"), "/lyrics/") + ".html";
      if (QRegularExpression("^https:\\/\\/.*\\/lyrics\\/.*\\/.*\\.html$").match(song_link).hasMatch()) {
        QUrl url(song_link);
        if (url.isValid()) {
          SendLyricsRequest(id, request, result_artist, result_album, result_title, url);
          return;
        }
      }
    }

    SendLyricsRequest(id, request, result_artist, result_album, result_title);

    return;

  }

  qLog(Debug) << "Lyrics.com: No lyrics for" << request.artist << request.album << request.title;
  emit SearchFinished(id);

}

void LyricsComLyricsProvider::CreateLyricsRequest(const int id, const LyricsSearchRequest &request) {

  SendLyricsRequest(id, request, request.artist, request.album, request.title);

}

void LyricsComLyricsProvider::SendLyricsRequest(const int id, const LyricsSearchRequest &request, const QString &result_artist, const QString &result_album, const QString &result_title, QUrl url) {

  if (url.isEmpty() || !url.isValid()) {
    url.setUrl(kLyricsUrl + StringFixup(result_artist) + "/" + StringFixup(result_title) + ".html");
  }

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request, result_artist, result_album, result_title]() { HandleLyricsReply(reply, id, request, result_artist, result_album, result_title); });

}

void LyricsComLyricsProvider::HandleLyricsReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request, const QString &result_artist, const QString &result_album, const QString &result_title) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << reply->errorString() << reply->error();
    emit SearchFinished(id);
    return;
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    qLog(Error) << "Received HTTP code" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    emit SearchFinished(id);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    qLog(Error) << "Lyrics.com: Empty reply received from server.";
    emit SearchFinished(id);
    return;
  }

  const QString lyrics = ParseLyricsFromHTML(QString::fromUtf8(data), QRegularExpression("<div[^>]*>"), QRegularExpression("<\\/div>"), QRegularExpression("<div id=\"lyrics\"[^>]+>"), false);
  if (lyrics.isEmpty() || lyrics.contains("Click to search for the Lyrics on Lyrics.com", Qt::CaseInsensitive)) {
    qLog(Debug) << "Lyrics.com: No lyrics for" << request.artist << request.album << request.title;
    emit SearchFinished(id);
    return;
  }

  qLog(Debug) << "Lyrics.com: Got lyrics for" << request.artist << request.album << request.title;

  LyricsSearchResult result(lyrics);
  result.artist = result_artist;
  result.album = result_album;
  result.title = result_title;
  emit SearchFinished(id, LyricsSearchResults() << result);

}

QString LyricsComLyricsProvider::StringFixup(QString string) {

  return string.replace('/', '-')
    .replace('\'', '-')
    .remove(QRegularExpression("[^\\w0-9\\- ]", QRegularExpression::UseUnicodePropertiesOption))
    .simplified()
    .replace(' ', '-')
    .replace(QRegularExpression("(-)\\1+"), "-")
    .toLower();

}

void LyricsComLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Lyrics.com:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
