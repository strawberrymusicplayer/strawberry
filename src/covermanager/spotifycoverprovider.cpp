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
#include "utilities/randutils.h"
#include "utilities/timeconstants.h"
#include "internet/internetservices.h"
#include "spotify/spotifyservice.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "spotifycoverprovider.h"

const int SpotifyCoverProvider::kLimit = 10;

SpotifyCoverProvider::SpotifyCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider("Spotify", true, true, 2.5, true, true, app, network, parent),
      service_(app->internet_services()->Service<SpotifyService>()) {}

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
    type = "track";
    extract = "tracks";
    if (!query.isEmpty()) query.append(" ");
    query.append(title);
  }
  else {
    type = "album";
    extract = "albums";
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(" ");
      query.append(album);
    }
  }

  ParamList params = ParamList() << Param("q", query)
                                 << Param("type", type)
                                 << Param("limit", QString::number(kLimit));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(SpotifyService::kApiUrl + QString("/search"));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
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
      Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      QString error;
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error") && json_obj["error"].isObject()) {
          QJsonObject obj_error = json_obj["error"].toObject();
          if (obj_error.contains("status") && obj_error.contains("message")) {
            int status = obj_error["status"].toInt();
            QString message = obj_error["message"].toString();
            error = QString("%1 (%2)").arg(message).arg(status);
            if (status == 401) Deauthenticate();
          }
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          if (reply->error() == 204) Deauthenticate();
          error = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
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
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  if (!json_obj.contains(extract) || !json_obj[extract].isObject()) {
    Error(QString("Json object is missing %1 object.").arg(extract), json_obj);
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }
  json_obj = json_obj[extract].toObject();

  if (!json_obj.contains("items") || !json_obj["items"].isArray()) {
    Error(QString("%1 object is missing items array.").arg(extract), json_obj);
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QJsonArray array_items = json_obj["items"].toArray();
  if (array_items.isEmpty()) {
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  CoverProviderSearchResults results;
  for (const QJsonValueRef value_item : array_items) {

    if (!value_item.isObject()) {
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    QJsonObject obj_album = obj_item;
    if (obj_item.contains("album") && obj_item["album"].isObject()) {
      obj_album = obj_item["album"].toObject();
    }

    if (!obj_album.contains("artists") || !obj_album.contains("name") || !obj_album.contains("images") || !obj_album["artists"].isArray() || !obj_album["images"].isArray()) {
      continue;
    }
    QJsonArray array_artists = obj_album["artists"].toArray();
    QJsonArray array_images = obj_album["images"].toArray();
    QString album = obj_album["name"].toString();

    QStringList artists;
    for (const QJsonValueRef value_artist : array_artists) {
      if (!value_artist.isObject()) continue;
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains("name")) continue;
      artists << obj_artist["name"].toString();
    }

    for (const QJsonValueRef value_image : array_images) {
      if (!value_image.isObject()) continue;
      QJsonObject obj_image = value_image.toObject();
      if (!obj_image.contains("url") || !obj_image.contains("width") || !obj_image.contains("height")) continue;
      int width = obj_image["width"].toInt();
      int height = obj_image["height"].toInt();
      if (width < 300 || height < 300) continue;
      QUrl url(obj_image["url"].toString());
      CoverProviderSearchResult result;
      result.album = album;
      result.image_url = url;
      result.image_size = QSize(width, height);
      if (!artists.isEmpty()) result.artist = artists.first();
      results << result;
    }

  }
  emit SearchFinished(id, results);

}

void SpotifyCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Spotify:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
