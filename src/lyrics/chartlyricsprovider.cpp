/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QList>
#include <QPair>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>

#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/utilities.h"
#include "lyricsprovider.h"
#include "lyricsfetcher.h"
#include "chartlyricsprovider.h"

const char *ChartLyricsProvider::kUrlSearch = "http://api.chartlyrics.com/apiv1.asmx/SearchLyricDirect";
const int ChartLyricsProvider::kMaxLength = 6000;

ChartLyricsProvider::ChartLyricsProvider(QObject *parent) : LyricsProvider("ChartLyrics", parent), network_(new NetworkAccessManager(this)) {}

bool ChartLyricsProvider::StartSearch(const QString &artist, const QString&, const QString &title, const quint64 id) {

  const ParamList params = ParamList() << Param("artist", artist)
                                       << Param("song", title);

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kUrlSearch);
  url.setQuery(url_query);
  QNetworkReply *reply = network_->get(QNetworkRequest(url));
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply*, const quint64, const QString&, const QString&)), reply, id, artist, title);

  //qLog(Debug) << "ChartLyrics: Sending request for" << url;

  return true;

}

void ChartLyricsProvider::CancelSearch(const quint64) {
}

void ChartLyricsProvider::HandleSearchReply(QNetworkReply *reply, const quint64 id, const QString &artist, const QString &title) {

  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    Error(id, failure_reason);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    QString failure_reason = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    Error(id, failure_reason);
    return;
  }

  QXmlStreamReader reader(reply);
  LyricsSearchResults results;
  LyricsSearchResult result;

  while (!reader.atEnd()) {
    QXmlStreamReader::TokenType type = reader.readNext();
    QStringRef name = reader.name();
    if (type == QXmlStreamReader::StartElement) {
      if (name == "GetLyricResult") {
        result = LyricsSearchResult();
      }
      if (name == "LyricArtist") {
        result.artist = reader.readElementText();
      }
      else if (name == "LyricSong") {
        result.title = reader.readElementText();
      }
      else if (name == "Lyric") {
        result.lyrics = reader.readElementText();
      }
    }
    else if (type == QXmlStreamReader::EndElement) {
       if (name == "GetLyricResult") {
         if (!result.artist.isEmpty() && !result.title.isEmpty() && !result.lyrics.isEmpty()) {
           result.score = 0.0;
           if (result.artist.toLower() == artist.toLower()) result.score += 1.0;
           if (result.title.toLower() == title.toLower()) result.score += 1.0;
           if (result.lyrics.length() > LyricsFetcher::kGoodLyricsLength) result.score += 1.0;
           if (result.artist.toLower() == artist.toLower() || result.title.toLower() == title.toLower()) {
             results << result;
           }
         }
         result = LyricsSearchResult();
       }
    }
  }

  if (results.isEmpty()) qLog(Debug) << "ChartLyrics: No lyrics for" << artist << title;
  else qLog(Debug) << "ChartLyrics: Got lyrics for" << artist << title;

  emit SearchFinished(id, results);

}

void ChartLyricsProvider::Error(const quint64 id, const QString &error, QVariant debug) {
  qLog(Error) << "ChartLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
  emit SearchFinished(id, LyricsSearchResults());
}
