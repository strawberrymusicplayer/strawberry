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

#include <QApplication>
#include <QThread>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "jsonlyricsprovider.h"
#include "ovhlyricsprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kUrlSearch[] = "https://api.lyrics.ovh/v1/";
}

OVHLyricsProvider::OVHLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(QStringLiteral("Lyrics.ovh"), true, false, network, parent) {}

OVHLyricsProvider::~OVHLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

void OVHLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  QUrl url(QString::fromLatin1(kUrlSearch) + QString::fromLatin1(QUrl::toPercentEncoding(request.artist)) + QLatin1Char('/') + QString::fromLatin1(QUrl::toPercentEncoding(request.title)));
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

}

void OVHLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QJsonObject json_obj = ExtractJsonObj(reply);
  if (json_obj.isEmpty()) {
    Q_EMIT SearchFinished(id);
    return;
  }

  if (json_obj.contains("error"_L1)) {
    Error(json_obj["error"_L1].toString());
    qLog(Debug) << "OVHLyrics: No lyrics for" << request.artist << request.title;
    Q_EMIT SearchFinished(id);
    return;
  }

  if (!json_obj.contains("lyrics"_L1)) {
    Q_EMIT SearchFinished(id);
    return;
  }

  LyricsSearchResult result;
  result.lyrics = json_obj["lyrics"_L1].toString();

  if (result.lyrics.isEmpty()) {
    qLog(Debug) << "OVHLyrics: No lyrics for" << request.artist << request.title;
    Q_EMIT SearchFinished(id);
  }
  else {
    result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);
    qLog(Debug) << "OVHLyrics: Got lyrics for" << request.artist << request.title;
    Q_EMIT SearchFinished(id, LyricsSearchResults() << result);
 }

}

void OVHLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "OVHLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
