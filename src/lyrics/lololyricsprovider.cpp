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
#include <QPair>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricsprovider.h"
#include "lyricsfetcher.h"
#include "lololyricsprovider.h"

const char *LoloLyricsProvider::kUrlSearch = "http://api.lololyrics.com/0.5/getLyric";

LoloLyricsProvider::LoloLyricsProvider(NetworkAccessManager *network, QObject *parent) : LyricsProvider("LoloLyrics", true, false, network, parent) {}

LoloLyricsProvider::~LoloLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool LoloLyricsProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

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
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, artist, title]() { HandleSearchReply(reply, id, artist, title); });

  //qLog(Debug) << "LoloLyrics: Sending request for" << url;

  return true;

}

void LoloLyricsProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void LoloLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &artist, const QString &title) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
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
      QString name = reader.name().toString();
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
            result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);
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
