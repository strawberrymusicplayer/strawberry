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

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "jsonlyricsprovider.h"
#include "ovhlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrlSearch[] = "https://api.lyrics.ovh/v1/";
}

OVHLyricsProvider::OVHLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(u"Lyrics.ovh"_s, true, false, network, parent) {}

void OVHLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  const QUrl url(QLatin1String(kUrlSearch) + QString::fromLatin1(QUrl::toPercentEncoding(request.artist)) + '/'_L1 + QString::fromLatin1(QUrl::toPercentEncoding(request.title)));
  QNetworkReply *reply = CreateGetRequest(url);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

}

void OVHLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  LyricsSearchResults results;
  const QScopeGuard search_finished = qScopeGuard([this, id, &results]() { Q_EMIT SearchFinished(id, results); });

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const QJsonObject json_object = GetJsonObject(reply).json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (json_object.contains("error"_L1)) {
    Error(json_object["error"_L1].toString());
    qLog(Debug) << "OVHLyrics: No lyrics for" << request.artist << request.title;
    return;
  }

  if (!json_object.contains("lyrics"_L1)) {
    return;
  }

  const QString lyrics = json_object["lyrics"_L1].toString();

  if (lyrics.isEmpty()) {
    qLog(Debug) << "OVHLyrics: No lyrics for" << request.artist << request.title;
  }
  else {
    qLog(Debug) << "OVHLyrics: Got lyrics for" << request.artist << request.title;
    results << LyricsSearchResult(Utilities::DecodeHtmlEntities(lyrics));
 }

}
