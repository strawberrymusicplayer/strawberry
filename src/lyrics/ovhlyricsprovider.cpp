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
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonValue>
#include <QtDebug>

#include "core/logging.h"
#include "core/network.h"
#include "lyricsfetcher.h"
#include "jsonlyricsprovider.h"
#include "ovhlyricsprovider.h"

const char *OVHLyricsProvider::kUrlSearch = "https://api.lyrics.ovh/v1/";

OVHLyricsProvider::OVHLyricsProvider(QObject *parent) : JsonLyricsProvider("Lyrics.ovh", true, false, parent), network_(new NetworkAccessManager(this)) {}

OVHLyricsProvider::~OVHLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool OVHLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const quint64 id) {

  Q_UNUSED(album);

  QUrl url(kUrlSearch + QString(QUrl::toPercentEncoding(artist)) + "/" + QString(QUrl::toPercentEncoding(title)));
  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, id, artist, title); });

  //qLog(Debug) << "OVHLyrics: Sending request for" << url;

  return true;

}

void OVHLyricsProvider::CancelSearch(const quint64 id) { Q_UNUSED(id); }

void OVHLyricsProvider::HandleSearchReply(QNetworkReply *reply, const quint64 id, const QString &artist, const QString &title) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QJsonObject json_obj = ExtractJsonObj(reply);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, LyricsSearchResults());
    return;
  }

  if (json_obj.contains("error")) {
    Error(json_obj["error"].toString());
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

  if (result.lyrics.isEmpty()) {
    qLog(Debug) << "OVHLyrics: No lyrics for" << artist << title;
    emit SearchFinished(id, LyricsSearchResults());
  }
  else {
    qLog(Debug) << "OVHLyrics: Got lyrics for" << artist << title;
    emit SearchFinished(id, LyricsSearchResults() << result);
 }

}


void OVHLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "OVHLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
