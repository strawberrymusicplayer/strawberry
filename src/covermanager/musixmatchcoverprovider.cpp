/*
 * Strawberry Music Player
 * Copyright 2020-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "utilities/musixmatchprovider.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "musixmatchcoverprovider.h"

using namespace Qt::Literals::StringLiterals;
using namespace MusixmatchProvider;

MusixmatchCoverProvider::MusixmatchCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Musixmatch"_s, true, false, 1.0, true, false, network, parent) {}

bool MusixmatchCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  Q_UNUSED(title);

  if (artist.isEmpty() || album.isEmpty()) return false;

  const QString artist_stripped = StringFixup(artist);
  const QString album_stripped = StringFixup(album);

  if (artist_stripped.isEmpty() || album_stripped.isEmpty()) return false;

  QNetworkReply *reply = CreateGetRequest(QUrl(QStringLiteral("https://www.musixmatch.com/album/%1/%2").arg(artist_stripped, album_stripped)));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, artist, album]() { HandleSearchReply(reply, id, artist, album); });

  //qLog(Debug) << "Musixmatch: Sending request for" << artist_stripped << album_stripped << url;

  return true;

}

void MusixmatchCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void MusixmatchCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &artist, const QString &album) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverProviderSearchResults results;
  const QScopeGuard search_finished = qScopeGuard([this, id, &results]() { Q_EMIT SearchFinished(id, results);; });

  const ReplyDataResult reply_data_result = GetReplyData(reply);
  if (!reply_data_result.success()) {
    Error(reply_data_result.error_message);
    return;
  }

  const QByteArray &data = reply_data_result.data;
  const QString content = QString::fromUtf8(data);
  const QString data_begin = "<script id=\"__NEXT_DATA__\" type=\"application/json\">"_L1;
  const QString data_end = "</script>"_L1;
  if (!content.contains(data_begin) || !content.contains(data_end)) {
    return;
  }
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
    return;
  }

  static const QRegularExpression regex_html_tag(u"<[^>]*>"_s);
  if (content_json.contains(regex_html_tag)) {  // Make sure it's not HTML code.
    return;
  }

  const JsonObjectResult json_object_result = GetJsonObject(content_json.toUtf8());
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  QJsonObject json_object = json_object_result.json_object;
  if (!json_object.contains("props"_L1) || !json_object["props"_L1].isObject()) {
    Error(u"Json reply is missing props."_s, json_object);
    return;
  }
  json_object = json_object["props"_L1].toObject();

  if (!json_object.contains("pageProps"_L1) || !json_object["pageProps"_L1].isObject()) {
    Error(u"Json props is missing pageProps."_s, json_object);
    Q_EMIT SearchFinished(id, results);
    return;
  }
  json_object = json_object["pageProps"_L1].toObject();

  if (!json_object.contains("data"_L1) || !json_object["data"_L1].isObject()) {
    Error(u"Json pageProps is missing data."_s, json_object);
    return;
  }
  json_object = json_object["data"_L1].toObject();

  if (!json_object.contains("albumGet"_L1) || !json_object["albumGet"_L1].isObject()) {
    Error(u"Json data is missing albumGet."_s, json_object);
    return;
  }
  json_object = json_object["albumGet"_L1].toObject();

  if (!json_object.contains("data"_L1) || !json_object["data"_L1].isObject()) {
    Error(u"Json albumGet reply is missing data."_s, json_object);
    return;
  }
  json_object = json_object["data"_L1].toObject();

  CoverProviderSearchResult result;
  if (json_object.contains("artistName"_L1) && json_object["artistName"_L1].isString()) {
    result.artist = json_object["artistName"_L1].toString();
  }
  if (json_object.contains("name"_L1) && json_object["name"_L1].isString()) {
    result.album = json_object["name"_L1].toString();
  }

  if (result.artist.compare(artist, Qt::CaseInsensitive) != 0 && result.album.compare(album, Qt::CaseInsensitive) != 0) {
    return;
  }

  const QList<QPair<QString, QSize>> cover_sizes = QList<QPair<QString, QSize>>() << qMakePair(u"coverImage800x800"_s, QSize(800, 800))
                                                                                  << qMakePair(u"coverImage500x500"_s, QSize(500, 500))
                                                                                  << qMakePair(u"coverImage350x350"_s, QSize(350, 350));

  for (const QPair<QString, QSize> &cover_size : cover_sizes) {
    if (!json_object.contains(cover_size.first)) continue;
    const QUrl cover_url(json_object[cover_size.first].toString());
    if (cover_url.isValid()) {
      result.image_url = cover_url;
      result.image_size = cover_size.second;
      results << result;
    }
  }

}

void MusixmatchCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Musixmatch:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
