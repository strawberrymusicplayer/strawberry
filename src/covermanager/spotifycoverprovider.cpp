/*
 * Strawberry Music Player
 * Copyright 2020-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QList>
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
#include <QDesktopServices>
#include <QMessageBox>

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/localredirectserver.h"
#include "utilities/randutils.h"
#include "utilities/timeconstants.h"
#include "streaming/streamingservices.h"
#include "spotify/spotifyservice.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "spotifycoverprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kApiUrl[] = "https://api.spotify.com/v1";
constexpr int kLimit = 10;
}  // namespace

SpotifyCoverProvider::SpotifyCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(QStringLiteral("Spotify"), true, true, 2.5, true, true, app, network, parent),
      service_(app->streaming_services()->Service<SpotifyService>()) {}

SpotifyCoverProvider::~SpotifyCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool SpotifyCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (!IsAuthenticated()) return false;

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString type;
  QString extract;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    type = "track"_L1;
    extract = "tracks"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    type = "album"_L1;
    extract = "albums"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  const ParamList params = ParamList() << Param(QStringLiteral("q"), query)
                                       << Param(QStringLiteral("type"), type)
                                       << Param(QStringLiteral("limit"), QString::number(kLimit));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(kApiUrl) + QStringLiteral("/search"));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  req.setRawHeader("Authorization", "Bearer " + service_->access_token().toUtf8());

  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, extract]() { HandleSearchReply(reply, id, extract); });

  return true;

}

void SpotifyCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

QByteArray SpotifyCoverProvider::GetReplyData(QNetworkReply *reply) {

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
      data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      QString error;
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error"_L1) && json_obj["error"_L1].isObject()) {
          QJsonObject obj_error = json_obj["error"_L1].toObject();
          if (obj_error.contains("status"_L1) && obj_error.contains("message"_L1)) {
            int status = obj_error["status"_L1].toInt();
            QString message = obj_error["message"_L1].toString();
            error = QStringLiteral("%1 (%2)").arg(message).arg(status);
            if (status == 401) Deauthenticate();
          }
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          if (reply->error() == 204) Deauthenticate();
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

void SpotifyCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &extract) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  if (!json_obj.contains(extract) || !json_obj[extract].isObject()) {
    Error(QStringLiteral("Json object is missing %1 object.").arg(extract), json_obj);
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }
  json_obj = json_obj[extract].toObject();

  if (!json_obj.contains("items"_L1) || !json_obj["items"_L1].isArray()) {
    Error(QStringLiteral("%1 object is missing items array.").arg(extract), json_obj);
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  const QJsonArray array_items = json_obj["items"_L1].toArray();
  if (array_items.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  CoverProviderSearchResults results;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    QJsonObject obj_album = obj_item;
    if (obj_item.contains("album"_L1) && obj_item["album"_L1].isObject()) {
      obj_album = obj_item["album"_L1].toObject();
    }

    if (!obj_album.contains("artists"_L1) || !obj_album.contains("name"_L1) || !obj_album.contains("images"_L1) || !obj_album["artists"_L1].isArray() || !obj_album["images"_L1].isArray()) {
      continue;
    }
    const QJsonArray array_artists = obj_album["artists"_L1].toArray();
    const QJsonArray array_images = obj_album["images"_L1].toArray();
    QString album = obj_album["name"_L1].toString();

    QStringList artists;
    for (const QJsonValue &value_artist : array_artists) {
      if (!value_artist.isObject()) continue;
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains("name"_L1)) continue;
      artists << obj_artist["name"_L1].toString();
    }

    for (const QJsonValue &value_image : array_images) {
      if (!value_image.isObject()) continue;
      QJsonObject obj_image = value_image.toObject();
      if (!obj_image.contains("url"_L1) || !obj_image.contains("width"_L1) || !obj_image.contains("height"_L1)) continue;
      int width = obj_image["width"_L1].toInt();
      int height = obj_image["height"_L1].toInt();
      if (width < 300 || height < 300) continue;
      QUrl url(obj_image["url"_L1].toString());
      CoverProviderSearchResult result;
      result.album = album;
      result.image_url = url;
      result.image_size = QSize(width, height);
      if (!artists.isEmpty()) result.artist = artists.first();
      results << result;
    }

  }
  Q_EMIT SearchFinished(id, results);

}

void SpotifyCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Spotify:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
