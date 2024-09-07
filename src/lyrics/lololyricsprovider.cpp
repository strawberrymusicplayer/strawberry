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
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/strutils.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "lololyricsprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kUrlSearch[] = "http://api.lololyrics.com/0.5/getLyric";
}

LoloLyricsProvider::LoloLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent) : LyricsProvider(QStringLiteral("LoloLyrics"), true, false, network, parent) {}

LoloLyricsProvider::~LoloLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

void LoloLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  QUrlQuery url_query;
  url_query.addQueryItem(QStringLiteral("artist"), QString::fromLatin1(QUrl::toPercentEncoding(request.artist)));
  url_query.addQueryItem(QStringLiteral("track"), QString::fromLatin1(QUrl::toPercentEncoding(request.title)));

  QUrl url(QString::fromLatin1(kUrlSearch));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

}

void LoloLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QString failure_reason;
  if (reply->error() != QNetworkReply::NoError) {
    failure_reason = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    if (reply->error() < 200) {
      Error(failure_reason);
      Q_EMIT SearchFinished(id);
      return;
    }
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    failure_reason = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
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
            failure_reason = reader.readElementText();
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
    qLog(Debug) << "LoloLyrics: No lyrics for" << request.artist << request.title << failure_reason;
  }
  else {
    qLog(Debug) << "LoloLyrics: Got lyrics for" << request.artist << request.title;
  }

  Q_EMIT SearchFinished(id, results);

}

void LoloLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "LoloLyrics:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
