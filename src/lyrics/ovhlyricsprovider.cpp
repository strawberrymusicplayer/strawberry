/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QJsonObject>

#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/utilities.h"
#include "lyricsprovider.h"
#include "lyricsfetcher.h"
#include "jsonlyricsprovider.h"
#include "ovhlyricsprovider.h"

const char *OVHLyricsProvider::kUrlSearch = "https://api.lyrics.ovh/v1/";

OVHLyricsProvider::OVHLyricsProvider(QObject *parent) : JsonLyricsProvider("Lyrics.ovh", parent), network_(new NetworkAccessManager(this)) {}

bool OVHLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const quint64 id) {

  QUrl url(kUrlSearch + QString(QUrl::toPercentEncoding(artist)) + "/" + QString(QUrl::toPercentEncoding(title)));
  QNetworkRequest req(url);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  QNetworkReply *reply = network_->get(req);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply*, const quint64, const QString&, const QString&)), reply, id, artist, title);

  //qLog(Debug) << "OVHLyrics: Sending request for" << url;

  return true;

}

void OVHLyricsProvider::CancelSearch(quint64 id) {}

void OVHLyricsProvider::HandleSearchReply(QNetworkReply *reply, const quint64 id, const QString &artist, const QString &title) {

  reply->deleteLater();

  QJsonObject json_obj = ExtractJsonObj(reply, id);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }

  if (json_obj.contains("error")) {
    Error(id, json_obj["error"].toString());
    qLog(Debug) << "OVHLyrics: No lyrics for" << artist << title;
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }

  if (!json_obj.contains("lyrics")) {
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }

  LyricsSearchResult result;
  result.lyrics = json_obj["lyrics"].toString();
  qLog(Debug) << "OVHLyrics: Got lyrics for" << artist << title;
  emit SearchFinished(id, LyricsSearchResults() << result);

}


void OVHLyricsProvider::Error(const quint64 id, const QString &error, QVariant debug) {

  qLog(Error) << "OVHLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
  emit SearchFinished(id, LyricsSearchResults());

}
