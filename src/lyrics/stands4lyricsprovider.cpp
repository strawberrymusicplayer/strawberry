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
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricsfetcher.h"
#include "lyricsprovider.h"
#include "stands4lyricsprovider.h"

const char *Stands4LyricsProvider::kUrl = "https://www.lyrics.com/lyrics/";

Stands4LyricsProvider::Stands4LyricsProvider(NetworkAccessManager *network, QObject *parent) : LyricsProvider("Stands4Lyrics", true, false, network, parent) {}

Stands4LyricsProvider::~Stands4LyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool Stands4LyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  Q_UNUSED(album);

  QUrl url(kUrl + StringFixup(artist) + "/" + StringFixup(title) + ".html");
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, artist, title]() { HandleSearchReply(reply, id, artist, title); });

  qLog(Debug) << "Stands4Lyrics: Sending request for" << url;

  return true;

}

void Stands4LyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void Stands4LyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &artist, const QString &title) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error("Empty reply received from server.");
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }

  const QString lyrics = ParseLyricsFromHTML(QString::fromUtf8(data), QRegularExpression("<div[^>]*>"), QRegularExpression("<\\/div>"), QRegularExpression("<div id=\"lyrics\"[^>]+>"), false);
  if (lyrics.isEmpty() || lyrics.contains("Click to search for the Lyrics on Lyrics.com", Qt::CaseInsensitive)) {
    qLog(Debug) << "Stands4Lyrics: No lyrics for" << artist << title;
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }

  qLog(Debug) << "Stands4Lyrics: Got lyrics for" << artist << title;

  LyricsSearchResult result;
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
