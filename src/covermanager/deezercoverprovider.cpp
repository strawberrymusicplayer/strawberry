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

#include <QtGlobal>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QtDebug>

#include "core/application.h"
#include "core/closure.h"
#include "core/network.h"
#include "core/logging.h"
#include "core/song.h"
#include "albumcoverfetcher.h"
#include "coverprovider.h"
#include "deezercoverprovider.h"

const char *DeezerCoverProvider::kApiUrl = "https://api.deezer.com";
const int DeezerCoverProvider::kLimit = 10;

DeezerCoverProvider::DeezerCoverProvider(Application *app, QObject *parent): CoverProvider("Deezer", 2.0, true, app, parent), network_(new NetworkAccessManager(this)) {}

bool DeezerCoverProvider::StartSearch(const QString &artist, const QString &album, const int id) {

  typedef QPair<QString, QString> Param;
  typedef QList<Param> Params;
  typedef QPair<QByteArray, QByteArray> EncodedParam;

  const Params params = Params() << Param("output", "json")
                                 << Param("q", QString(artist + " " + album))
                                 << Param("limit", QString::number(kLimit));

  QUrlQuery url_query;
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(kApiUrl + QString("/search/album"));
  url.setQuery(url_query);
  QNetworkRequest req(url);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  QNetworkReply *reply = network_->get(req);

  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply*, int)), reply, id);

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
      QString error = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(error);
    }
    else {
      // See if there is Json data containing "error" object - then use that instead.
      data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      QString error;
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error")) {
          QJsonValue json_value_error = json_obj["error"];
          if (json_value_error.isObject()) {
            QJsonObject json_error = json_value_error.toObject();
            int code = json_error["code"].toInt();
            QString message = json_error["message"].toString();
            QString type = json_error["type"].toString();
            error = QString("%1 (%2)").arg(message).arg(code);
          }
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
  
QJsonObject DeezerCoverProvider::ExtractJsonObj(const QByteArray &data) {

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

QJsonValue DeezerCoverProvider::ExtractData(const QByteArray &data) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonObject();

  if (json_obj.contains("error")) {
    QJsonValue json_value_error = json_obj["error"];
    if (!json_value_error.isObject()) {
      Error("Error missing object", json_obj);
      return QJsonValue();
    }
    QJsonObject json_error = json_value_error.toObject();
    int code = json_error["code"].toInt();
    QString message = json_error["message"].toString();
    QString type = json_error["type"].toString();
    Error(QString("%1 (%2)").arg(message).arg(code));
    return QJsonValue();
  }

  if (!json_obj.contains("data") && !json_obj.contains("DATA")) {
    Error("Json reply is missing data.", json_obj);
    return QJsonValue();
  }

  QJsonValue json_data;
  if (json_obj.contains("data")) json_data = json_obj["data"];
  else json_data = json_obj["DATA"];

  return json_data;

}

void DeezerCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  reply->deleteLater();

  CoverSearchResults results;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue json_value = ExtractData(data);
  if (!json_value.isArray()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonArray json_data = json_value.toArray();
  if (json_data.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  for (const QJsonValue &value : json_data) {

    if (!value.isObject()) {
      Error("Invalid Json reply, data is not an object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (!json_obj.contains("id") || !json_obj.contains("type")) {
      Error("Invalid Json reply, item is missing ID or type.", json_obj);
      continue;
    }

    QString type = json_obj["type"].toString();
    if (type != "album") {
      Error("Invalid Json reply, incorrect type returned", json_obj);
      continue;
    }

    if (!json_obj.contains("artist")) {
      Error("Invalid Json reply, item missing artist.", json_obj);
      continue;
    }
    QJsonValue json_value_artist = json_obj["artist"];
    if (!json_value_artist.isObject()) {
      Error("Invalid Json reply, item artist is not a object.", json_value_artist);
      continue;
    }
    QJsonObject json_artist = json_value_artist.toObject();

    if (!json_artist.contains("name")) {
      Error("Invalid Json reply, artist data missing name.", json_artist);
      continue;
    }
    QString artist = json_artist["name"].toString();

    if (!json_obj.contains("title")) {
      Error("Invalid Json reply, data missing title.", json_obj);
      continue;
    }
    QString album = json_obj["title"].toString();

    QString cover;
    if (json_obj.contains("cover_xl")) {
      cover = json_obj["cover_xl"].toString();
    }
    else if (json_obj.contains("cover_big")) {
      cover = json_obj["cover_big"].toString();
    }
    else if (json_obj.contains("cover_medium")) {
      cover = json_obj["cover_medium"].toString();
    }
    else if (json_obj.contains("cover_small")) {
      cover = json_obj["cover_small"].toString();
    }
    else {
      Error("Invalid Json reply, data missing cover.", json_obj);
      continue;
    }
    QUrl url(cover);

    album.remove(Song::kAlbumRemoveDisc);
    album.remove(Song::kAlbumRemoveMisc);

    CoverSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = url;
    results << cover_result;

  }
  emit SearchFinished(id, results);

}

void DeezerCoverProvider::Error(const QString &error, const QVariant &debug) {
  qLog(Error) << "Deezer:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
}
