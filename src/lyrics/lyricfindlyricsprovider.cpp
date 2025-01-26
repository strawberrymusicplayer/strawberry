/*
 * Strawberry Music Player
 * Copyright 2024-2025, Jonas Kvinge <jonas@jkvinge.net>
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

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kUrl[] = "https://lyrics.lyricfind.com/lyrics";
constexpr char kLyricsStart[] = "<script id=\"__NEXT_DATA__\" type=\"application/json\">";
constexpr char kLyricsEnd[] = "</script>";
}  // namespace

LyricFindLyricsProvider::LyricFindLyricsProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent) : JsonLyricsProvider(u"lyricfind.com"_s, true, false, network, parent) {}

QUrl LyricFindLyricsProvider::Url(const LyricsSearchRequest &request) {

  return QUrl(QLatin1String(kUrl) + QLatin1Char('/') + StringFixup(request.artist) + QLatin1Char('-') + StringFixup(request.title));

}

QString LyricFindLyricsProvider::StringFixup(const QString &text) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  static const QRegularExpression regex_illegal_characters(u"[^\\w0-9_\\- ]"_s);
  static const QRegularExpression regex_multiple_whitespaces(u" {2,}"_s);

  return Utilities::Transliterate(text)
    .remove(regex_illegal_characters)
    .replace(regex_multiple_whitespaces, u" "_s)
    .simplified()
    .replace(u' ', u'-')
    .toLower();

}

void LyricFindLyricsProvider::StartSearch(const int id, const LyricsSearchRequest &request) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  const QUrl url = Url(request);
  QNetworkReply *reply = CreateGetRequest(url, true);
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

  const ReplyDataResult reply_data_result = GetReplyData(reply);
  if (!reply_data_result.success()) {
    Error(reply_data_result.error_message);
    return;
  }

  const QByteArray &data = reply_data_result.data;
  if (data.isEmpty()) {
    Error(u"Empty reply received from server."_s);
    return;
  }

  const QString content = QString::fromUtf8(data);
  if (content.isEmpty()) {
    Error(u"Empty reply received from server."_s);
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
    Error(u"Could not parse HTML reply."_s);
    return;
  }

  const JsonObjectResult json_object_result = GetJsonObject(content_json.toUtf8());
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("props"_L1) || !json_object["props"_L1].isObject()) {
    Error(u"Missing props."_s);
    return;
  }
  json_object = json_object["props"_L1].toObject();
  if (!json_object.contains("pageProps"_L1) || !json_object["pageProps"_L1].isObject()) {
    Error(u"Missing pageProps."_s);
    return;
  }
  json_object = json_object["pageProps"_L1].toObject();
  if (!json_object.contains("songData"_L1) || !json_object["songData"_L1].isObject()) {
    Error(u"Missing songData."_s);
    return;
  }
  json_object = json_object["songData"_L1].toObject();

  if (!json_object.contains("response"_L1) || !json_object["response"_L1].isObject()) {
    Error(u"Missing response."_s);
    return;
  }
  //const QJsonObject obj_response = obj[QLatin1String("response")].toObject();

  if (!json_object.contains("track"_L1) || !json_object["track"_L1].isObject()) {
    Error(u"Missing track."_s);
    return;
  }
  const QJsonObject object_track = json_object["track"_L1].toObject();

  if (!object_track.contains("title"_L1) ||
      !object_track.contains("lyrics"_L1)) {
    Error(u"Missing title or lyrics."_s);
    return;
  }

  LyricsSearchResult result;

  const QJsonObject object_artist = json_object["artist"_L1].toObject();
  if (object_artist.contains("name"_L1)) {
    result.artist = object_artist["name"_L1].toString();
  }
  result.title = object_track["title"_L1].toString();
  result.lyrics = object_track["lyrics"_L1].toString();
  results << result;

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
