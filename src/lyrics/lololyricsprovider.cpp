/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include "lololyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrlSearch[] = "http://api.lololyrics.com/0.5/getLyric";
}

LoloLyricsProvider::LoloLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent) : LyricsProvider(u"LoloLyrics"_s, true, false, network, parent) {}

void LoloLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  QUrlQuery url_query;
  url_query.addQueryItem(u"artist"_s, QString::fromLatin1(QUrl::toPercentEncoding(request.artist)));
  url_query.addQueryItem(u"track"_s, QString::fromLatin1(QUrl::toPercentEncoding(request.title)));

  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(kUrlSearch)), url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

}

void LoloLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

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

  QString error_message = reply_data_result.error_message;

  if (!reply_data_result.data.isEmpty()) {
    QXmlStreamReader reader(reply_data_result.data);
    LyricsSearchResult result;
    QString status;
    while (!reader.atEnd()) {
      QXmlStreamReader::TokenType type = reader.readNext();
      QString name = reader.name().toString();
      if (type == QXmlStreamReader::StartElement) {
        if (name == "result"_L1) {
          status.clear();
          result = LyricsSearchResult();
        }
        else if (name == "status"_L1) {
          status = reader.readElementText();
        }
        else if (name == "response"_L1) {
          if (status == "OK"_L1) {
            result.lyrics = reader.readElementText();
          }
          else {
            error_message = reader.readElementText();
            result = LyricsSearchResult();
          }
        }
      }
      else if (type == QXmlStreamReader::EndElement) {
        if (name == "result"_L1) {
          if (!result.lyrics.isEmpty()) {
            result.lyrics = Utilities::DecodeHtmlEntities(result.lyrics);
            results << result;
          }
          result = LyricsSearchResult();
        }
      }
    }
  }

  if (results.isEmpty()) {
    qLog(Debug) << "LoloLyrics: No lyrics for" << request.artist << request.title << error_message;
  }
  else {
    qLog(Debug) << "LoloLyrics: Got lyrics for" << request.artist << request.title;
  }

}
