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

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "songlyricscomlyricsprovider.h"

const char *SongLyricsComLyricsProvider::kUrl = "https://www.songlyrics.com/";

SongLyricsComLyricsProvider::SongLyricsComLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent) : LyricsProvider("songlyrics.com", true, false, network, parent) {}

SongLyricsComLyricsProvider::~SongLyricsComLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool SongLyricsComLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  SendRequest(id, request, request.artist, request.album, request.title);

  return true;

}

void SongLyricsComLyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void SongLyricsComLyricsProvider::SendRequest(const int id, const LyricsSearchRequest &request, const QString &result_artist, const QString &result_album, const QString &result_title, QUrl url) {

  if (url.isEmpty() || !url.isValid()) {
    url.setUrl(kUrl + StringFixup(result_artist) + "/" + StringFixup(result_title) + "-lyrics/");
  }

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request, result_artist, result_album, result_title]() { HandleLyricsReply(reply, id, request, result_artist, result_album, result_title); });

}

void SongLyricsComLyricsProvider::HandleLyricsReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request, const QString &result_artist, const QString &result_album, const QString &result_title) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << "songlyrics.com:" << reply->errorString() << reply->error();
    emit SearchFinished(id);
    return;
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    qLog(Error) << "songlyrics.com: Received HTTP code" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    emit SearchFinished(id);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    qLog(Error) << "songlyrics.com: Empty reply received from server.";
    emit SearchFinished(id);
    return;
  }

  const QString lyrics = ParseLyricsFromHTML(QString::fromUtf8(data), QRegularExpression("<p[^>]*>"), QRegularExpression("<\\/p>"), QRegularExpression("<p id=\"songLyricsDiv\"[^>]+>"), false);
  if (lyrics.isEmpty()) {
    qLog(Debug) << "songlyrics.com: No lyrics for" << request.artist << request.album << request.title;
    emit SearchFinished(id);
    return;
  }

  qLog(Debug) << "songlyrics.com: Got lyrics for" << request.artist << request.album << request.title;

  LyricsSearchResult result(lyrics);
  result.artist = result_artist;
  result.album = result_album;
  result.title = result_title;
  emit SearchFinished(id, LyricsSearchResults() << result);

}

QString SongLyricsComLyricsProvider::StringFixup(QString string) {

  return string.replace('/', '-')
    .replace('\'', '-')
    .remove(QRegularExpression("[^\\w0-9\\- ]", QRegularExpression::UseUnicodePropertiesOption))
    .simplified()
    .replace(' ', '-')
    .replace(QRegularExpression("(-)\\1+"), "-")
    .toLower();

}

void SongLyricsComLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "songlyrics.com:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
