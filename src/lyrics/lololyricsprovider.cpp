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
#include <QPair>
#include <QList>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QtDebug>

#include "core/logging.h"
#include "core/network.h"
#include "lyricsprovider.h"
#include "lyricsfetcher.h"
#include "lololyricsprovider.h"

const char *LoloLyricsProvider::kUrlSearch = "http://api.lololyrics.com/0.5/getLyric";

LoloLyricsProvider::LoloLyricsProvider(QObject *parent) : LyricsProvider("LoloLyrics", true, false, parent), network_(new NetworkAccessManager(this)) {}

bool LoloLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const quint64 id) {

  Q_UNUSED(album);

  const ParamList params = ParamList() << Param("artist", artist)
                                       << Param("track", title);

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kUrlSearch);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  QNetworkReply *reply = network_->get(req);
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, id, artist, title); });

  //qLog(Debug) << "LoloLyrics: Sending request for" << url;

  return true;

}

void LoloLyricsProvider::CancelSearch(const quint64 id) { Q_UNUSED(id); }

void LoloLyricsProvider::HandleSearchReply(QNetworkReply *reply, const quint64 id, const QString &artist, const QString &title) {

  reply->deleteLater();

  QString failure_reason;
  if (reply->error() != QNetworkReply::NoError) {
    failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    if (reply->error() < 200) {
      Error(failure_reason);
      emit SearchFinished(id, LyricsSearchResults());
      return;
    }
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    failure_reason = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
  }

  QByteArray data = reply->readAll();
  LyricsSearchResults results;

  if (!data.isEmpty()) {
    QXmlStreamReader reader(data);
    LyricsSearchResult result;
    QString status;
    while (!reader.atEnd()) {
      QXmlStreamReader::TokenType type = reader.readNext();
      QStringRef name = reader.name();
      if (type == QXmlStreamReader::StartElement) {
        if (name == "result") {
          status.clear();
          result = LyricsSearchResult();
        }
        else if (name == "status") {
          status = reader.readElementText();
        }
        else if (name == "response") {
          if (status == "OK") {
            result.lyrics = reader.readElementText();
          }
          else {
            failure_reason = reader.readElementText();
            result = LyricsSearchResult();
          }
        }
      }
      else if (type == QXmlStreamReader::EndElement) {
         if (name == "result") {
           if (!result.lyrics.isEmpty()) {
             results << result;
           }
           result = LyricsSearchResult();
         }
      }
    }
  }

  if (results.isEmpty()) qLog(Debug) << "LoloLyrics: No lyrics for" << artist << title << failure_reason;
  else qLog(Debug) << "LoloLyrics: Got lyrics for" << artist << title;

  emit SearchFinished(id, results);

}

void LoloLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "LoloLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
