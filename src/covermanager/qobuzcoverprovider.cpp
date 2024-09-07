/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/song.h"
#include "streaming/streamingservices.h"
#include "qobuz/qobuzservice.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "qobuzcoverprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr int kLimit = 10;
}

QobuzCoverProvider::QobuzCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(QStringLiteral("Qobuz"), true, true, 2.0, true, true, app, network, parent),
      service_(app->streaming_services()->Service<QobuzService>()) {}

QobuzCoverProvider::~QobuzCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool QobuzCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString resource;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    resource = "track/search"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    resource = "album/search"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  ParamList params = ParamList() << Param(QStringLiteral("query"), query)
                                 << Param(QStringLiteral("limit"), QString::number(kLimit))
                                 << Param(QStringLiteral("app_id"), service_->app_id());

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(QobuzService::kApiUrl) + QLatin1Char('/') + resource);
  url.setQuery(url_query);

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  req.setRawHeader("X-App-Id", service_->app_id().toUtf8());
  req.setRawHeader("X-User-Auth-Token", service_->user_auth_token().toUtf8());
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { HandleSearchReply(reply, id); });

  return true;

}

void QobuzCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

QByteArray QobuzCoverProvider::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "status", "code" and "message" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status"_L1) && json_obj.contains("code"_L1) && json_obj.contains("message"_L1)) {
          int code = json_obj["code"_L1].toInt();
          QString message = json_obj["message"_L1].toString();
          error = QStringLiteral("%1 (%2)").arg(message).arg(code);
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

void QobuzCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverProviderSearchResults results;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    Q_EMIT SearchFinished(id, results);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    Q_EMIT SearchFinished(id, results);
    return;
  }

  QJsonValue value_type;
  if (json_obj.contains("albums"_L1)) {
    value_type = json_obj["albums"_L1];
  }
  else if (json_obj.contains("tracks"_L1)) {
    value_type = json_obj["tracks"_L1];
  }
  else {
    Error(QStringLiteral("Json reply is missing albums and tracks object."), json_obj);
    Q_EMIT SearchFinished(id, results);
    return;
  }

  if (!value_type.isObject()) {
    Error(QStringLiteral("Json albums or tracks is not a object."), value_type);
    Q_EMIT SearchFinished(id, results);
    return;
  }
  QJsonObject obj_type = value_type.toObject();

  if (!obj_type.contains("items"_L1)) {
    Error(QStringLiteral("Json albums or tracks object does not contain items."), obj_type);
    Q_EMIT SearchFinished(id, results);
    return;
  }
  QJsonValue value_items = obj_type["items"_L1];

  if (!value_items.isArray()) {
    Error(QStringLiteral("Json albums or track object items is not a array."), value_items);
    Q_EMIT SearchFinished(id, results);
    return;
  }
  const QJsonArray array_items = value_items.toArray();

  for (const QJsonValue &value : array_items) {

    if (!value.isObject()) {
      Error(QStringLiteral("Invalid Json reply, value in items is not a object."));
      continue;
    }
    QJsonObject item_obj = value.toObject();

    QJsonObject obj_album;
    if (item_obj.contains("album"_L1)) {
      if (!item_obj["album"_L1].isObject()) {
        Error(QStringLiteral("Invalid Json reply, items album is not a object."), item_obj);
        continue;
      }
      obj_album = item_obj["album"_L1].toObject();
    }
    else {
      obj_album = item_obj;
    }

    if (!obj_album.contains("artist"_L1) || !obj_album.contains("image"_L1) || !obj_album.contains("title"_L1)) {
      Error(QStringLiteral("Invalid Json reply, item is missing artist, title or image."), obj_album);
      continue;
    }

    QString album = obj_album["title"_L1].toString();

    // Artist
    QJsonValue value_artist = obj_album["artist"_L1];
    if (!value_artist.isObject()) {
      Error(QStringLiteral("Invalid Json reply, items (album) artist is not a object."), value_artist);
      continue;
    }
    QJsonObject obj_artist = value_artist.toObject();
    if (!obj_artist.contains("name"_L1)) {
      Error(QStringLiteral("Invalid Json reply, items (album) artist is missing name."), obj_artist);
      continue;
    }
    QString artist = obj_artist["name"_L1].toString();

    // Image
    QJsonValue value_image = obj_album["image"_L1];
    if (!value_image.isObject()) {
      Error(QStringLiteral("Invalid Json reply, items (album) image is not a object."), value_image);
      continue;
    }
    QJsonObject obj_image = value_image.toObject();
    if (!obj_image.contains("large"_L1)) {
      Error(QStringLiteral("Invalid Json reply, items (album) image is missing large."), obj_image);
      continue;
    }
    QUrl cover_url(obj_image["large"_L1].toString());

    CoverProviderSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = Song::AlbumRemoveDiscMisc(album);
    cover_result.image_url = cover_url;
    cover_result.image_size = QSize(600, 600);
    results << cover_result;

  }
  Q_EMIT SearchFinished(id, results);

}

void QobuzCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Qobuz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
