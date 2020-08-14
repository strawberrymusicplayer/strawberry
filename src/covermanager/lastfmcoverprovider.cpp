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

#include <algorithm>

#include <QtGlobal>
#include <QObject>
#include <QLocale>
#include <QList>
#include <QPair>
#include <QSet>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QtDebug>

#include "core/application.h"
#include "core/network.h"
#include "core/logging.h"

#include "jsoncoverprovider.h"
#include "albumcoverfetcher.h"
#include "lastfmcoverprovider.h"

const char *LastFmCoverProvider::kUrl = "https://ws.audioscrobbler.com/2.0/";
const char *LastFmCoverProvider::kApiKey = "211990b4c96782c05d1536e7219eb56e";
const char *LastFmCoverProvider::kSecret = "80fd738f49596e9709b1bf9319c444a8";

LastFmCoverProvider::LastFmCoverProvider(Application *app, QObject *parent) : JsonCoverProvider("Last.fm", true, false, 1.0, true, false, app, parent), network_(new NetworkAccessManager(this)) {}

LastFmCoverProvider::~LastFmCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool LastFmCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString method;
  QString type;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    method = "track.search";
    type = "track";
    if (!query.isEmpty()) query.append(" ");
    query.append(title);
  }
  else {
    method = "album.search";
    type = "album";
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(" ");
      query.append(album);
    }
  }

  ParamList params = ParamList() << Param("api_key", kApiKey)
                                 << Param("lang", QLocale().name().left(2).toLower())
                                 << Param("method", method)
                                 << Param(type, query);

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  QString data_to_sign;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    data_to_sign += param.first + param.second;
  }
  data_to_sign += kSecret;

  QByteArray const digest = QCryptographicHash::hash(data_to_sign.toUtf8(), QCryptographicHash::Md5);
  QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, '0').toLower();

  url_query.addQueryItem(QUrl::toPercentEncoding("api_sig"), QUrl::toPercentEncoding(signature));
  url_query.addQueryItem(QUrl::toPercentEncoding("format"), QUrl::toPercentEncoding("json"));

  QUrl url(kUrl);
  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QNetworkReply *reply = network_->post(req, url_query.toString(QUrl::FullyEncoded).toUtf8());
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { QueryFinished(reply, id, type); });

  return true;

}

void LastFmCoverProvider::QueryFinished(QNetworkReply *reply, const int id, const QString &type) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverSearchResults results;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue value_results;
  if (json_obj.contains("results")) {
    value_results = json_obj["results"];
  }
  else if (json_obj.contains("error") && json_obj.contains("message")) {
    int error = json_obj["error"].toInt();
    QString message = json_obj["message"].toString();
    Error(QString("Error: %1: %2").arg(QString::number(error)).arg(message));
    emit SearchFinished(id, results);
    return;
  }
  else {
    Error(QString("Json reply is missing results."), json_obj);
    emit SearchFinished(id, results);
    return;
  }

  if (!value_results.isObject()) {
    Error("Json results is not a object.", value_results);
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject obj_results = value_results.toObject();
  if (obj_results.isEmpty()) {
    Error("Json results object is empty.", value_results);
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue value_matches;

  if (type == "album") {
    if (obj_results.contains("albummatches")) {
      value_matches = obj_results["albummatches"];
    }
    else {
      Error("Json results object is missing albummatches.", obj_results);
      emit SearchFinished(id, results);
      return;
    }
  }
  else if (type == "track") {
    if (obj_results.contains("trackmatches")) {
      value_matches = obj_results["trackmatches"];
    }
    else {
      Error("Json results object is missing trackmatches.", obj_results);
      emit SearchFinished(id, results);
      return;
    }
  }

  if (!value_matches.isObject()) {
    Error("Json albummatches or trackmatches is not an object.", value_matches);
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject obj_matches = value_matches.toObject();
  if (obj_matches.isEmpty()) {
    Error("Json albummatches or trackmatches object is empty.", value_matches);
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue value_type;
  if (!obj_matches.contains(type)) {
    Error(QString("Json object is missing %1.").arg(type), obj_matches);
    emit SearchFinished(id, results);
    return;
  }
  value_type = obj_matches[type];

  if (!value_type.isArray()) {
    Error("Json album value in albummatches object is not an array.", value_type);
    emit SearchFinished(id, results);
    return;
  }
  QJsonArray array_type = value_type.toArray();

  for (const QJsonValue &value : array_type) {

    if (!value.isObject()) {
      Error("Invalid Json reply, value in albummatches/trackmatches array is not a object.", value);
      continue;
    }
    QJsonObject obj = value.toObject();
    if (!obj.contains("artist") || !obj.contains("image") || !obj.contains("name")) {
      Error("Invalid Json reply, album is missing artist, image or name.", obj);
      continue;
    }
    QString artist = obj["artist"].toString();
    QString album;
    if (type == "album") {
      album = obj["name"].toString();
    }

    QJsonValue json_image = obj["image"];
    if (!json_image.isArray()) {
      Error("Invalid Json reply, album image is not a array.", json_image);
      continue;
    }
    QJsonArray array_image = json_image.toArray();
    QUrl url;
    LastFmImageSize size(LastFmImageSize::Unknown);
    for (const QJsonValue &value_image : array_image) {
      if (!value_image.isObject()) {
        Error("Invalid Json reply, album image value is not an object.", value_image);
        continue;
      }
      QJsonObject obj_image = value_image.toObject();
      if (!obj_image.contains("#text") || !obj_image.contains("size")) {
        Error("Invalid Json reply, album image value is missing #text or size.", obj_image);
        continue;
      }
      QString image_url = obj_image["#text"].toString();
      LastFmImageSize image_size = ImageSizeFromString(obj_image["size"].toString().toLower());
      if (url.isEmpty() || image_size > size) {
        url.setUrl(image_url);
        size = image_size;
      }
    }

    if (!url.isValid()) continue;

    CoverSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = url;
    cover_result.image_size = QSize(size, size);
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

void LastFmCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Last.fm:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

LastFmCoverProvider::LastFmImageSize LastFmCoverProvider::ImageSizeFromString(const QString &size) {

  if (size == "small") return LastFmImageSize::Small;
  else if (size == "medium") return LastFmImageSize::Medium;
  else if (size == "large") return LastFmImageSize::Large;
  else if (size == "extralarge") return LastFmImageSize::ExtraLarge;
  else return LastFmImageSize::Unknown;

}
