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
#include "lyricsfetcher.h"
#include "stands4lyricsprovider.h"

const char *Stands4LyricsProvider::kApiUrl = "https://www.abbreviations.com/services/v2/lyrics.php";
const char *Stands4LyricsProvider::kLyricsUrl = "https://www.lyrics.com/lyrics/";
const char *Stands4LyricsProvider::kUID = "11363";
const char *Stands4LyricsProvider::kTokenB64 = "b3FOYmxhV1ZKRGxIMnV4OA==";

Stands4LyricsProvider::Stands4LyricsProvider(NetworkAccessManager *network, QObject *parent) : JsonLyricsProvider("Stands4Lyrics", true, false, network, parent), api_usage_exceeded_(false) {}

Stands4LyricsProvider::~Stands4LyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool Stands4LyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (api_usage_exceeded_) {
    SendLyricsRequest(id, artist, album, title);
  }
  else {
    SendSearchRequest(id, artist, album, title);
  }

  return true;

}

void Stands4LyricsProvider::SendSearchRequest(const int id, const QString &artist, const QString &album, const QString &title) {

  QUrlQuery url_query;
  url_query.addQueryItem(QUrl::toPercentEncoding("uid"), QUrl::toPercentEncoding(kUID));
  url_query.addQueryItem(QUrl::toPercentEncoding("tokenid"), QUrl::toPercentEncoding(QByteArray::fromBase64(kTokenB64)));
  url_query.addQueryItem(QUrl::toPercentEncoding("format"), "json");
  url_query.addQueryItem(QUrl::toPercentEncoding("artist"), QUrl::toPercentEncoding(artist));
  url_query.addQueryItem(QUrl::toPercentEncoding("album"), QUrl::toPercentEncoding(album));
  url_query.addQueryItem(QUrl::toPercentEncoding("term"), QUrl::toPercentEncoding(title));
  QUrl url(kApiUrl);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, artist, album, title]() { HandleSearchReply(reply, id, artist, album, title); });

}

void Stands4LyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void Stands4LyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &artist, const QString &album, const QString &title) {

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
    Error(QString("Failed to parse json data: %1").arg(json_error.errorString()));
    emit SearchFinished(id);
    return;
  }

  if (json_doc.isEmpty()) {
    qLog(Debug) << "Stands4Lyrics: No lyrics for" << artist << album << title;
    emit SearchFinished(id);
    return;
  }

  if (!json_doc.isObject()) {
    Error("Json document is not an object.", json_doc);
    emit SearchFinished(id);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    qLog(Debug) << "Stands4Lyrics: No lyrics for" << artist << album << title;
    emit SearchFinished(id);
    return;
  }

  if (json_obj.contains("error")) {
    const QString error = json_obj["error"].toString();
    if (error.compare("Daily Usage Exceeded", Qt::CaseInsensitive) == 0) {
      api_usage_exceeded_ = true;
      SendLyricsRequest(id, artist, album, title);
      return;
    }
    Error(error);
    emit SearchFinished(id);
    return;
  }

  if (!json_obj.contains("result") || !json_obj["result"].isArray()) {
    Error("Json reply is missing result.", json_obj);
    emit SearchFinished(id);
    return;
  }

  QJsonArray json_result = json_obj["result"].toArray();
  if (json_result.isEmpty()) {
    qLog(Debug) << "Stands4Lyrics: No lyrics for" << artist << album << title;
    emit SearchFinished(id);
    return;
  }

  LyricsSearchResults results;
  for (const QJsonValueRef value : json_result) {
    if (!value.isObject()) {
      qLog(Error) << "Stands4Lyrics: Invalid Json reply, result is not an object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject obj = value.toObject();
    if (
      !obj.contains("song") ||
      !obj.contains("artist") ||
      !obj.contains("album") ||
      !obj.contains("song-link")
    ) {
      qLog(Error) << "Stands4Lyrics: Invalid Json reply, result is missing data.";
      qLog(Debug) << value;
      continue;
    }
    QString result_artist = obj["artist"].toString();
    QString result_album = obj["album"].toString();
    QString result_title = obj["song"].toString();
    QString song_link = obj["song-link"].toString();
    if (result_artist.compare(artist, Qt::CaseInsensitive) != 0 &&
        result_album.compare(album, Qt::CaseInsensitive) != 0 &&
        result_title.compare(title, Qt::CaseInsensitive) != 0) {
      continue;
    }

    if (!song_link.isEmpty() && QRegularExpression("^https:\\/\\/.*\\/lyric\\/\\d+\\/.*\\/.*$").match(song_link).hasMatch()) {
      song_link = song_link.replace(QRegularExpression("\\/lyric\\/\\d+\\/"), "/lyrics/") + ".html";
      if (QRegularExpression("^https:\\/\\/.*\\/lyrics\\/.*\\/.*\\.html$").match(song_link).hasMatch()) {
        QUrl url(song_link);
        if (url.isValid()) {
          SendLyricsRequest(id, result_artist, result_album, result_title, url);
          return;
        }
      }
    }

    SendLyricsRequest(id, result_artist, result_album, result_title);

    return;

  }

  qLog(Debug) << "Stands4Lyrics: No lyrics for" << artist << album << title;
  emit SearchFinished(id);

}

void Stands4LyricsProvider::SendLyricsRequest(const int id, const QString &artist, const QString &album, const QString &title, QUrl url) {

  if (url.isEmpty() || !url.isValid()) {
    url.setUrl(kLyricsUrl + StringFixup(artist) + "/" + StringFixup(title) + ".html");
  }

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, artist, album, title]() { HandleLyricsReply(reply, id, artist, album, title); });

}

void Stands4LyricsProvider::HandleLyricsReply(QNetworkReply *reply, const int id, const QString &artist, const QString &album, const QString &title) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    emit SearchFinished(id);
    return;
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    emit SearchFinished(id);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error("Empty reply received from server.");
    emit SearchFinished(id);
    return;
  }

  const QString lyrics = ParseLyricsFromHTML(QString::fromUtf8(data), QRegularExpression("<div[^>]*>"), QRegularExpression("<\\/div>"), QRegularExpression("<div id=\"lyrics\"[^>]+>"), false);
  if (lyrics.isEmpty() || lyrics.contains("Click to search for the Lyrics on Lyrics.com", Qt::CaseInsensitive)) {
    qLog(Debug) << "Stands4Lyrics: No lyrics for" << artist << album << title;
    emit SearchFinished(id);
    return;
  }

  qLog(Debug) << "Stands4Lyrics: Got lyrics for" << artist << album << title;

  LyricsSearchResult result;
  result.artist = artist;
  result.album = album;
  result.title = title;
  result.lyrics = lyrics;
  emit SearchFinished(id, LyricsSearchResults() << result);

}

QString Stands4LyricsProvider::StringFixup(QString string) {

  return string.replace('/', '-')
    .replace('\'', '-')
    .remove(QRegularExpression("[^\\w0-9\\- ]", QRegularExpression::UseUnicodePropertiesOption))
    .simplified()
    .replace(' ', '-')
    .replace(QRegularExpression("(-)\\1+"), "-")
    .toLower();

}

void Stands4LyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Stands4Lyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
