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

#include <QObject>
#include <QList>
#include <QPair>
#include <QByteArray>
#include <QString>
#include <QStringBuilder>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include "core/application.h"
#include "core/closure.h"
#include "core/network.h"
#include "core/logging.h"

#include "coverprovider.h"
#include "albumcoverfetcher.h"
#include "lastfmcoverprovider.h"

const char *LastFmCoverProvider::kUrl = "https://ws.audioscrobbler.com/2.0/";
const char *LastFmCoverProvider::kApiKey = "211990b4c96782c05d1536e7219eb56e";
const char *LastFmCoverProvider::kSecret = "80fd738f49596e9709b1bf9319c444a8";

LastFmCoverProvider::LastFmCoverProvider(Application *app, QObject *parent) : CoverProvider("last.fm", 1.0, true, app, parent), network_(new NetworkAccessManager(this)) {}

bool LastFmCoverProvider::StartSearch(const QString &artist, const QString &album, const int id) {

  typedef QPair<QString, QString> Param;
  typedef QPair<QByteArray, QByteArray> EncodedParam;
  typedef QList<Param> ParamList;

  ParamList params = ParamList()
    << Param("album", QString(artist + " " + album))
    << Param("api_key", kApiKey)
    << Param("lang", QLocale().name().left(2).toLower())
    << Param("method", "album.search");

  QUrlQuery url_query;
  QString data_to_sign;
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
    data_to_sign += param.first + param.second;
  }
  data_to_sign += kSecret;

  QByteArray const digest = QCryptographicHash::hash(data_to_sign.toUtf8(), QCryptographicHash::Md5);
  QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, '0').toLower();

  url_query.addQueryItem(QUrl::toPercentEncoding("api_sig"), QUrl::toPercentEncoding(signature));
  url_query.addQueryItem(QUrl::toPercentEncoding("format"), QUrl::toPercentEncoding("json"));

  QUrl url(kUrl);
  QNetworkRequest req(url);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QNetworkReply *reply = network_->post(req, url_query.toString(QUrl::FullyEncoded).toUtf8());
  NewClosure(reply, SIGNAL(finished()), this, SLOT(QueryFinished(QNetworkReply*, int)), reply, id);

  return true;

}

void LastFmCoverProvider::QueryFinished(QNetworkReply *reply, const int id) {

  reply->deleteLater();

  CoverSearchResults results;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue json_value = ExtractResults(data);
  if (!json_value.isObject()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject json_results = json_value.toObject();
  if (json_results.isEmpty()) {
    Error("Json object is empty.", json_value);
    emit SearchFinished(id, results);
    return;
  }

  if (!json_results.contains("albummatches")) {
    Error("Json results is missing albummatches.", json_results);
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue json_matches = json_results["albummatches"];
  if (!json_matches.isObject()) {
    Error("Json albummatches is not an object.", json_matches);
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject json_obj_matches = json_matches.toObject();
  if (json_obj_matches.isEmpty()) {
    Error("Json albummatches object is empty.", json_matches);
    emit SearchFinished(id, results);
    return;
  }
  if (!json_obj_matches.contains("album")) {
    Error("Json albummatches is missing album.", json_obj_matches);
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue json_album = json_obj_matches["album"];
  if (!json_album.isArray()) {
    Error("Json album is not an array.", json_album);
    emit SearchFinished(id, results);
    return;
  }
  QJsonArray json_array = json_album.toArray();

  for (const QJsonValue &value : json_array) {
    if (!value.isObject()) {
      Error("Invalid Json reply, album value is not an object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();
    if (!json_obj.contains("artist") || !json_obj.contains("image") || !json_obj.contains("name")) {
      Error("Invalid Json reply, album is missing artist, image or name.", json_obj);
      continue;
    }
    QString artist = json_obj["artist"].toString();
    QString album = json_obj["name"].toString();

    QJsonValue json_image = json_obj["image"];
    if (!json_image.isArray()) {
      Error("Invalid Json reply, album image is not an array.", json_image);
      continue;
    }
    QJsonArray json_array_image = json_image.toArray();
    QUrl url;
    LastFmImageSize size(LastFmImageSize::Unknown);
    for (QJsonValue json_value_image : json_array_image) {
      if (!json_value_image.isObject()) {
        Error("Invalid Json reply, album image value is not an object.", json_value_image);
        continue;
      }
      QJsonObject json_object_image = json_value_image.toObject();
      if (!json_object_image.contains("#text") || !json_object_image.contains("size")) {
       Error("Invalid Json reply, album image value is missing #text or size.", json_object_image);
        continue;
      }
      QString image_url = json_object_image["#text"].toString();
      LastFmImageSize image_size = ImageSizeFromString(json_object_image["size"].toString().toLower());
      if (url.isEmpty() || image_size > size) {
        url.setUrl(image_url);
        size = image_size;
      }
    }

    if (url.isEmpty()) continue;

    CoverSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = url;
    results << cover_result;
  }
  emit SearchFinished(id, results);

}

QByteArray LastFmCoverProvider::GetReplyData(QNetworkReply *reply) {

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
      // See if there is Json data containing "error" and "message" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error") && json_obj.contains("message")) {
          int code = json_obj["error"].toInt();
          QString message = json_obj["message"].toString();
          error = "Error: " + QString::number(code) + ": " + message;
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
      Error(error);
    }
    return QByteArray();
  }

  return data;

}

QJsonObject LastFmCoverProvider::ExtractJsonObj(const QByteArray &data) {

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }
  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error("Received empty Json document.", json_doc);
    return QJsonObject();
  }
  if (!json_doc.isObject()) {
    Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }
  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error("Received empty Json object.", json_doc);
    return QJsonObject();
  }

  return json_obj;

}

QJsonValue LastFmCoverProvider::ExtractResults(const QByteArray &data) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonObject();

  if (json_obj.contains("results")) {
    QJsonValue json_results = json_obj["results"];
    return json_results;
  }
  else if (json_obj.contains("error") && json_obj.contains("message")) {
    int error = json_obj["error"].toInt();
    QString message = json_obj["message"].toString();
    Error(QString("Error: %1: %2").arg(QString::number(error)).arg(message));
  }
  else {
    Error(QString("Json reply is missing results."), json_obj);
  }
  return QJsonValue();

}

void LastFmCoverProvider::Error(const QString &error, const QVariant &debug) {
  qLog(Error) << "LastFm:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
}

LastFmCoverProvider::LastFmImageSize LastFmCoverProvider::ImageSizeFromString(const QString &size) {
  if (size == "small") return LastFmImageSize::Small;
  else if (size == "medium") return LastFmImageSize::Medium;
  else if (size == "large") return LastFmImageSize::Large;
  else if (size == "extralarge") return LastFmImageSize::ExtraLarge;
  else return LastFmImageSize::Unknown;
}
