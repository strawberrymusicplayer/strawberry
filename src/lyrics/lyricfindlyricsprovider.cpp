/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/transliterate.h"
#include "lyricfindlyricsprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kUrl[] = "https://lyrics.lyricfind.com/lyrics";
constexpr char kLyricsStart[] = "<script id=\"__NEXT_DATA__\" type=\"application/json\">";
constexpr char kLyricsEnd[] = "</script>";
}  // namespace

LyricFindLyricsProvider::LyricFindLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(QStringLiteral("lyricfind.com"), true, false, network, parent) {}

LyricFindLyricsProvider::~LyricFindLyricsProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

QUrl LyricFindLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QLatin1String(kUrl) + QLatin1Char('/') + StringFixup(request.artist) + QLatin1Char('-') + StringFixup(request.title));

}

QString LyricFindLyricsProvider::StringFixup(const QString &text) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  static const QRegularExpression regex_illegal_characters(QStringLiteral("[^\\w0-9_\\- ]"));
  static const QRegularExpression regex_multiple_whitespaces(QStringLiteral(" {2,}"));

  return Utilities::Transliterate(text)
    .remove(regex_illegal_characters)
    .replace(regex_multiple_whitespaces, QStringLiteral(" "))
    .simplified()
    .replace(u' ', u'-')
    .toLower();

}

void LyricFindLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  const QUrl url = Url(request);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (X11; Linux x86_64; rv:122.0) Gecko/20100101 Firefox/122.0"));
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, request]() { HandleSearchReply(reply, id, request); });

  qLog(Debug) << "LyricFind: Sending request for" << url;

}

void LyricFindLyricsProvider::HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  LyricsSearchResults results;
  const QScopeGuard end_search = qScopeGuard([this, id, request, &results]() { EndSearch(id, request, results); });

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    const int http_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http_code != 200 && http_code != 201 && http_code != 202) {
      Error(QStringLiteral("Received HTTP code %1").arg(http_code));
      return;
    }
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Empty reply received from server."));
    return;
  }

  const QString content = QString::fromUtf8(data);
  if (content.isEmpty()) {
    Error(QStringLiteral("Empty reply received from server."));
    return;
  }

  const QString data_begin = QLatin1String(kLyricsStart);
  const QString data_end = QLatin1String(kLyricsEnd);
  qint64 begin_idx = content.indexOf(data_begin);
  QString content_json;
  if (begin_idx > 0) {
    begin_idx += data_begin.length();
    qint64 end_idx = content.indexOf(data_end, begin_idx);
    if (end_idx > begin_idx) {
      content_json = content.mid(begin_idx, end_idx - begin_idx);
    }
  }
  if (content_json.isEmpty()) {
    Error(QStringLiteral("Could not parse HTML reply."));
    return;
  }

  QJsonObject obj = ExtractJsonObj(content_json.toUtf8());
  if (obj.isEmpty()) {
    return;
  }
  if (!obj.contains("props"_L1) || !obj["props"_L1].isObject()) {
    Error(QStringLiteral("Missing props."));
    return;
  }
  obj = obj["props"_L1].toObject();
  if (!obj.contains("pageProps"_L1) || !obj["pageProps"_L1].isObject()) {
    Error(QStringLiteral("Missing pageProps."));
    return;
  }
  obj = obj["pageProps"_L1].toObject();
  if (!obj.contains("songData"_L1) || !obj["songData"_L1].isObject()) {
    Error(QStringLiteral("Missing songData."));
    return;
  }
  obj = obj["songData"_L1].toObject();

  if (!obj.contains("response"_L1) || !obj["response"_L1].isObject()) {
    Error(QStringLiteral("Missing response."));
    return;
  }
  //const QJsonObject obj_response = obj[QLatin1String("response")].toObject();

  if (!obj.contains("track"_L1) || !obj["track"_L1].isObject()) {
    Error(QStringLiteral("Missing track."));
    return;
  }
  const QJsonObject obj_track = obj["track"_L1].toObject();

  if (!obj_track.contains("title"_L1) ||
      !obj_track.contains("lyrics"_L1)) {
    Error(QStringLiteral("Missing title or lyrics."));
    return;
  }

  LyricsSearchResult result;

  const QJsonObject obj_artist = obj["artist"_L1].toObject();
  if (obj_artist.contains("name"_L1)) {
    result.artist = obj_artist["name"_L1].toString();
  }
  result.title = obj_track["title"_L1].toString();
  result.lyrics = obj_track["lyrics"_L1].toString();
  results << result;

}

void LyricFindLyricsProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "LyricFind:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

void LyricFindLyricsProvider::EndSearch(const int id, const LyricsSearchRequest &request, const LyricsSearchResults &results) {

  if (results.isEmpty()) {
    qLog(Debug) << "LyricFind: No lyrics for" << request.artist << request.title;
  }
  else {
    qLog(Debug) << "LyricFind: Got lyrics for" << request.artist << request.title;
  }

  Q_EMIT SearchFinished(id, results);

}
