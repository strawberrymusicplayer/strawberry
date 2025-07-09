/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "chartlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrlSearch[] = "http://api.chartlyrics.com/apiv1.asmx/SearchLyricDirect";
}

ChartLyricsProvider::ChartLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent) : LyricsProvider(u"ChartLyrics"_s, false, false, network, parent) {}

void ChartLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  QUrlQuery url_query;
  url_query.addQueryItem(u"artist"_s, QString::fromUtf8(QUrl::toPercentEncoding(request.artist)));
  url_query.addQueryItem(u"song"_s, QString::fromUtf8(QUrl::toPercentEncoding(request.title)));

  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(kUrlSearch)), url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

}

void ChartLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  LyricsSearchResults results;
  const QScopeGuard search_finished = qScopeGuard([this, id, &results]() { Q_EMIT SearchFinished(id, results); });

  const ReplyDataResult reply_data_result = GetReplyData(reply);
  if (!reply_data_result.success()) {
    Error(reply_data_result.error_message);
    return;
  }

  QXmlStreamReader reader(reply_data_result.data);
  LyricsSearchResult result;
  while (!reader.atEnd()) {
    const QXmlStreamReader::TokenType type = reader.readNext();
    const QString name = reader.name().toString();
    if (type == QXmlStreamReader::StartElement) {
      if (name == "GetLyricResult"_L1) {
        result = LyricsSearchResult();
      }
      if (name == "LyricArtist"_L1) {
        result.artist = reader.readElementText();
      }
      else if (name == "LyricSong"_L1) {
        result.title = reader.readElementText();
      }
      else if (name == "Lyric"_L1) {
        result.lyrics = reader.readElementText();
      }
    }
    else if (type == QXmlStreamReader::EndElement) {
      if (name == "GetLyricResult"_L1) {
        if (!result.artist.isEmpty() && !result.title.isEmpty() && !result.lyrics.isEmpty() &&
            (result.artist.compare(request.albumartist, Qt::CaseInsensitive) == 0 ||
             result.artist.compare(request.artist, Qt::CaseInsensitive) == 0 ||
             result.title.compare(request.title, Qt::CaseInsensitive) == 0)) {
          result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);
          results << result;
        }
        result = LyricsSearchResult();
      }
    }
  }

  if (results.isEmpty()) {
    qLog(Debug) << "ChartLyrics: No lyrics for" << request.artist << request.title;
  }
  else {
    qLog(Debug) << "ChartLyrics: Got lyrics for" << request.artist << request.title;
  }

}
