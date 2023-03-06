/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "jsonlyricsprovider.h"
#include "ovhlyricsprovider.h"

const char *OVHLyricsProvider::kUrlSearch = "https://api.lyrics.ovh/v1/";

OVHLyricsProvider::OVHLyricsProvider(NetworkAccessManager *network, QObject *parent) : JsonLyricsProvider("Lyrics.ovh", true, false, network, parent) {}

OVHLyricsProvider::~OVHLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool OVHLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  QUrl url(kUrlSearch + QString(QUrl::toPercentEncoding(request.artist)) + "/" + QString(QUrl::toPercentEncoding(request.title)));
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

  return true;

}

void OVHLyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void OVHLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QJsonObject json_obj = ExtractJsonObj(reply);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id);
    return;
  }

  if (json_obj.contains("error")) {
    Error(json_obj["error"].toString());
    qLog(Debug) << "OVHLyrics: No lyrics for" << request.artist << request.title;
    emit SearchFinished(id);
    return;
  }

  if (!json_obj.contains("lyrics")) {
    emit SearchFinished(id);
    return;
  }

  LyricsSearchResult result;
  result.lyrics = json_obj["lyrics"].toString();

  if (result.lyrics.isEmpty()) {
    qLog(Debug) << "OVHLyrics: No lyrics for" << request.artist << request.title;
    emit SearchFinished(id);
  }
  else {
    result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);
    qLog(Debug) << "OVHLyrics: Got lyrics for" << request.artist << request.title;
    emit SearchFinished(id, LyricsSearchResults() << result);
 }

}

void OVHLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "OVHLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
