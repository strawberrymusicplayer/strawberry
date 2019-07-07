/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <stdbool.h>

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

#include "core/application.h"
#include "core/closure.h"
#include "core/network.h"
#include "core/logging.h"
#include "internet/internetservices.h"
#include "settings/tidalsettingspage.h"
#include "tidal/tidalservice.h"
#include "albumcoverfetcher.h"
#include "coverprovider.h"
#include "tidalcoverprovider.h"

const char *TidalCoverProvider::kApiUrl = "https://api.tidalhifi.com/v1";
const char *TidalCoverProvider::kResourcesUrl = "http://resources.tidal.com";
const int TidalCoverProvider::kLimit = 10;

TidalCoverProvider::TidalCoverProvider(Application *app, QObject *parent) : 
  CoverProvider("Tidal", 2.0, true, app, parent),
  service_(app->internet_services()->Service<TidalService>()),
  network_(new NetworkAccessManager(this)) {

}

bool TidalCoverProvider::StartSearch(const QString &artist, const QString &album, const int id) {

  if (!service_ || !service_->authenticated()) return false;

  ParamList params = ParamList() << Param("query", QString(artist + " " + album))
                                 << Param("limit", QString::number(kLimit));

  QNetworkReply *reply = CreateRequest("search/albums", params);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply*, int)), reply, id);

  return true;

}

void TidalCoverProvider::CancelSearch(int id) {}

QNetworkReply *TidalCoverProvider::CreateRequest(const QString &ressource_name, const ParamList &params_supplied) {

  const ParamList params = ParamList() << params_supplied
                                       << Param("countryCode", service_->country_code());

  QUrlQuery url_query;
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(kApiUrl + QString("/") + ressource_name);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  if (!service_->access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + service_->access_token().toUtf8());
  if (!service_->session_id().isEmpty()) req.setRawHeader("X-Tidal-SessionId", service_->session_id().toUtf8());
  QNetworkReply *reply = network_->get(req);

  return reply;

}

QByteArray TidalCoverProvider::GetReplyData(QNetworkReply *reply, QString &error) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      error = Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "status" and "userMessage" - then use that instead.
      data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      int status = 0;
      int sub_status = 0;
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("userMessage")) {
          status = json_obj["status"].toInt();
          sub_status = json_obj["subStatus"].toInt();
          QString user_message = json_obj["userMessage"].toString();
          error = QString("%1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          error = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      if (status == 401 && sub_status == 6001) {  // User does not have a valid session
        service_->Logout();
      }
      error = Error(error);
    }
    return QByteArray();
  }

  return data;

}

QJsonObject TidalCoverProvider::ExtractJsonObj(QByteArray &data, QString &error) {

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    error = Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }

  if (json_doc.isEmpty()) {
    error = Error("Received empty Json document.", data);
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    error = Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    error = Error("Received empty Json object.", json_doc);
    return QJsonObject();
  }

  return json_obj;

}

QJsonValue TidalCoverProvider::ExtractItems(QByteArray &data, QString &error) {

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) return QJsonValue();
  return ExtractItems(json_obj, error);

}

QJsonValue TidalCoverProvider::ExtractItems(QJsonObject &json_obj, QString &error) {

  if (!json_obj.contains("items")) {
    error = Error("Json reply is missing items.", json_obj);
    return QJsonArray();
  }
  QJsonValue json_items = json_obj["items"];
  return json_items;

}

void TidalCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  reply->deleteLater();

  CoverSearchResults results;
  QString error;

  QByteArray data = GetReplyData(reply, error);
  if (data.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue json_value = ExtractItems(json_obj, error);
  if (!json_value.isArray()) {
    emit SearchFinished(id, results);
    return;
  }
  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  for (const QJsonValue &value : json_items) {
    if (!value.isObject()) {
      Error("Invalid Json reply, item not a object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (!json_obj.contains("artist") || !json_obj.contains("type") || !json_obj.contains("id") || !json_obj.contains("title") || !json_obj.contains("cover")) {
      Error("Invalid Json reply, item missing id, type, album or cover.", json_obj);
      continue;
    }
    QString album = json_obj["title"].toString();
    QString cover = json_obj["cover"].toString();

    QJsonValue json_value_artist = json_obj["artist"];
    if (!json_value_artist.isObject()) {
      Error("Invalid Json reply, item artist is not a object.", json_value_artist);
      continue;
    }
    QJsonObject json_artist = json_value_artist.toObject();
    if (!json_artist.contains("name")) {
      Error("Invalid Json reply, item artist missing name.", json_artist);
      continue;
    }
    QString artist = json_artist["name"].toString();

    album.remove(Song::kAlbumRemoveDisc);
    album.remove(Song::kAlbumRemoveMisc);

    cover = cover.replace("-", "/");
    QUrl cover_url (QString("%1/images/%2/%3.jpg").arg(kResourcesUrl).arg(cover).arg("1280x1280"));

    CoverSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = cover_url;
    results << cover_result;

  }
  emit SearchFinished(id, results);

}

QString TidalCoverProvider::Error(QString error, QVariant debug) {
  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
  return error;
}
