/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QPair>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/song.h"
#include "albumcoverfetcher.h"
#include "albumcoverfetchersearch.h"
#include "jsoncoverprovider.h"
#include "deezercoverprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kApiUrl[] = "https://api.deezer.com";
constexpr int kLimit = 10;
}

DeezerCoverProvider::DeezerCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(QStringLiteral("Deezer"), true, false, 2.0, true, true, app, network, parent) {}

DeezerCoverProvider::~DeezerCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool DeezerCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString resource;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    resource = "search/track"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    resource = "search/album"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  const ParamList params = ParamList() << Param(QStringLiteral("output"), QStringLiteral("json"))
                                       << Param(QStringLiteral("q"), query)
                                       << Param(QStringLiteral("limit"), QString::number(kLimit));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(kApiUrl) + QLatin1Char('/') + resource);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { HandleSearchReply(reply, id); });

  return true;

}

void DeezerCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

QByteArray DeezerCoverProvider::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(error);
    }
    else {
      // See if there is Json data containing "error" object - then use that instead.
      data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      QString error;
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error"_L1)) {
          QJsonValue value_error = json_obj["error"_L1];
          if (value_error.isObject()) {
            QJsonObject obj_error = value_error.toObject();
            int code = obj_error["code"_L1].toInt();
            QString message = obj_error["message"_L1].toString();
            error = QStringLiteral("%1 (%2)").arg(message).arg(code);
          }
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      Error(error);
    }
    return QByteArray();
  }

  return data;

}

QJsonValue DeezerCoverProvider::ExtractData(const QByteArray &data) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonObject();

  if (json_obj.contains("error"_L1)) {
    QJsonValue value_error = json_obj["error"_L1];
    if (!value_error.isObject()) {
      Error(QStringLiteral("Error missing object"), json_obj);
      return QJsonValue();
    }
    QJsonObject obj_error = value_error.toObject();
    const int code = obj_error["code"_L1].toInt();
    QString message = obj_error["message"_L1].toString();
    Error(QStringLiteral("%1 (%2)").arg(message).arg(code));
    return QJsonValue();
  }

  if (!json_obj.contains("data"_L1) && !json_obj.contains("DATA"_L1)) {
    Error(QStringLiteral("Json reply object is missing data."), json_obj);
    return QJsonValue();
  }

  QJsonValue value_data;
  if (json_obj.contains("data"_L1)) value_data = json_obj["data"_L1];
  else value_data = json_obj["DATA"_L1];

  return value_data;

}

void DeezerCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QJsonValue value_data = ExtractData(data);
  if (!value_data.isArray()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QJsonArray array_data = value_data.toArray();
  if (array_data.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QMap<QUrl, CoverProviderSearchResult> results;
  int i = 0;
  for (const QJsonValue &json_value : std::as_const(array_data)) {

    if (!json_value.isObject()) {
      Error(QStringLiteral("Invalid Json reply, data array value is not a object."));
      continue;
    }
    QJsonObject json_obj = json_value.toObject();
    QJsonObject obj_album;
    if (json_obj.contains("album"_L1) && json_obj["album"_L1].isObject()) {  // Song search, so extract the album.
      obj_album = json_obj["album"_L1].toObject();
    }
    else {
      obj_album = json_obj;
    }

    if (!json_obj.contains("id"_L1) || !obj_album.contains("id"_L1)) {
      Error(QStringLiteral("Invalid Json reply, data array value object is missing ID."), json_obj);
      continue;
    }

    if (!obj_album.contains("type"_L1)) {
      Error(QStringLiteral("Invalid Json reply, data array value album object is missing type."), obj_album);
      continue;
    }
    QString type = obj_album["type"_L1].toString();
    if (type != "album"_L1) {
      Error(QStringLiteral("Invalid Json reply, data array value album object has incorrect type returned"), obj_album);
      continue;
    }

    if (!json_obj.contains("artist"_L1)) {
      Error(QStringLiteral("Invalid Json reply, data array value object is missing artist."), json_obj);
      continue;
    }
    QJsonValue value_artist = json_obj["artist"_L1];
    if (!value_artist.isObject()) {
      Error(QStringLiteral("Invalid Json reply, data array value artist is not a object."), value_artist);
      continue;
    }
    QJsonObject obj_artist = value_artist.toObject();

    if (!obj_artist.contains("name"_L1)) {
      Error(QStringLiteral("Invalid Json reply, data array value artist object is missing name."), obj_artist);
      continue;
    }
    QString artist = obj_artist["name"_L1].toString();

    if (!obj_album.contains("title"_L1)) {
      Error(QStringLiteral("Invalid Json reply, data array value album object is missing title."), obj_album);
      continue;
    }
    QString album = obj_album["title"_L1].toString();

    CoverProviderSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = Song::AlbumRemoveDiscMisc(album);

    bool have_cover = false;
    const QList<QPair<QString, QSize>> cover_sizes = QList<QPair<QString, QSize>>() << qMakePair(QStringLiteral("cover_xl"), QSize(1000, 1000))
                                                                                    << qMakePair(QStringLiteral("cover_big"), QSize(500, 500));
    for (const QPair<QString, QSize> &cover_size : cover_sizes) {
      if (!obj_album.contains(cover_size.first)) continue;
      QString cover = obj_album[cover_size.first].toString();
      if (!have_cover) {
        have_cover = true;
        ++i;
      }
      QUrl url(cover);
      if (!results.contains(url)) {
        cover_result.image_url = url;
        cover_result.image_size = cover_size.second;
        cover_result.number = i;
        results.insert(url, cover_result);
      }
    }

    if (!have_cover) {
      Error(QStringLiteral("Invalid Json reply, data array value album object is missing cover."), obj_album);
    }

  }

  if (results.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
  }
  else {
    CoverProviderSearchResults cover_results = results.values();
    std::stable_sort(cover_results.begin(), cover_results.end(), AlbumCoverFetcherSearch::CoverProviderSearchResultCompareNumber);
    Q_EMIT SearchFinished(id, cover_results);
  }

}

void DeezerCoverProvider::Error(const QString &error, const QVariant &debug) {
  qLog(Error) << "Deezer:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
}
