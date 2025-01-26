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

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/song.h"
#include "qobuz/qobuzservice.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "qobuzcoverprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kLimit = 10;
}

QobuzCoverProvider::QobuzCoverProvider(const QobuzServicePtr service, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Qobuz"_s, true, true, 2.0, true, true, network, parent),
      service_(service) {}

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

  ParamList params = ParamList() << Param(u"query"_s, query)
                                 << Param(u"limit"_s, QString::number(kLimit))
                                 << Param(u"app_id"_s, service_->app_id());

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(QobuzService::kApiUrl) + QLatin1Char('/') + resource);
  url.setQuery(url_query);

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  req.setRawHeader("X-App-Id", service_->app_id().toUtf8());
  req.setRawHeader("X-User-Auth-Token", service_->user_auth_token().toUtf8());
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { HandleSearchReply(reply, id); });

  return true;

}

void QobuzCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void QobuzCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverProviderSearchResults results;

  const QByteArray data = GetReplyData(reply).data;
  if (data.isEmpty()) {
    Q_EMIT SearchFinished(id, results);
    return;
  }

  const QJsonObject json_object = ExtractJsonObj(data);
  if (json_object.isEmpty()) {
    Q_EMIT SearchFinished(id, results);
    return;
  }

  QJsonValue value_type;
  if (json_object.contains("albums"_L1)) {
    value_type = json_object["albums"_L1];
  }
  else if (json_object.contains("tracks"_L1)) {
    value_type = json_object["tracks"_L1];
  }
  else {
    Error(u"Json reply is missing albums and tracks object."_s, json_object);
    Q_EMIT SearchFinished(id, results);
    return;
  }

  if (!value_type.isObject()) {
    Error(u"Json albums or tracks is not a object."_s, value_type);
    Q_EMIT SearchFinished(id, results);
    return;
  }
  const QJsonObject object_type = value_type.toObject();

  if (!object_type.contains("items"_L1)) {
    Error(u"Json albums or tracks object does not contain items."_s, object_type);
    Q_EMIT SearchFinished(id, results);
    return;
  }
  const QJsonValue value_items = object_type["items"_L1];

  if (!value_items.isArray()) {
    Error(u"Json albums or track object items is not a array."_s, value_items);
    Q_EMIT SearchFinished(id, results);
    return;
  }
  const QJsonArray array_items = value_items.toArray();

  for (const QJsonValue &value : array_items) {

    if (!value.isObject()) {
      Error(u"Invalid Json reply, value in items is not a object."_s);
      continue;
    }
    const QJsonObject item_object = value.toObject();

    QJsonObject object_album;
    if (item_object.contains("album"_L1)) {
      if (!item_object["album"_L1].isObject()) {
        Error(u"Invalid Json reply, items album is not a object."_s, item_object);
        continue;
      }
      object_album = item_object["album"_L1].toObject();
    }
    else {
      object_album = item_object;
    }

    if (!object_album.contains("artist"_L1) || !object_album.contains("image"_L1) || !object_album.contains("title"_L1)) {
      Error(u"Invalid Json reply, item is missing artist, title or image."_s, object_album);
      continue;
    }

    const QString album = object_album["title"_L1].toString();

    // Artist
    const QJsonValue value_artist = object_album["artist"_L1];
    if (!value_artist.isObject()) {
      Error(u"Invalid Json reply, items (album) artist is not a object."_s, value_artist);
      continue;
    }
    const QJsonObject object_artist = value_artist.toObject();
    if (!object_artist.contains("name"_L1)) {
      Error(u"Invalid Json reply, items (album) artist is missing name."_s, object_artist);
      continue;
    }
    const QString artist = object_artist["name"_L1].toString();

    // Image
    const QJsonValue value_image = object_album["image"_L1];
    if (!value_image.isObject()) {
      Error(u"Invalid Json reply, items (album) image is not a object."_s, value_image);
      continue;
    }
    const QJsonObject object_image = value_image.toObject();
    if (!object_image.contains("large"_L1)) {
      Error(u"Invalid Json reply, items (album) image is missing large."_s, object_image);
      continue;
    }
    const QUrl cover_url(object_image["large"_L1].toString());

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
