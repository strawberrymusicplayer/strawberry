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

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/song.h"
#include "streaming/streamingservices.h"
#include "tidal/tidalservice.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "tidalcoverprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr int kLimit = 10;
}

TidalCoverProvider::TidalCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(QStringLiteral("Tidal"), true, true, 2.5, true, true, app, network, parent),
      service_(app->streaming_services()->Service<TidalService>()) {}

TidalCoverProvider::~TidalCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool TidalCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (!service_ || !service_->authenticated()) return false;

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString resource;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    resource = "search/tracks"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    resource = "search/albums"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  const ParamList params = ParamList() << Param(QStringLiteral("query"), query)
                                       << Param(QStringLiteral("limit"), QString::number(kLimit))
                                       << Param(QStringLiteral("countryCode"), service_->country_code());

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(TidalService::kApiUrl) + QLatin1Char('/') + resource);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  if (service_->oauth() && !service_->access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + service_->access_token().toUtf8());
  else if (!service_->session_id().isEmpty()) req.setRawHeader("X-Tidal-SessionId", service_->session_id().toUtf8());

  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id]() { HandleSearchReply(reply, id); });

  return true;

}

void TidalCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

QByteArray TidalCoverProvider::GetReplyData(QNetworkReply *reply) {

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
      // See if there is Json data containing "status" and "userMessage" - then use that instead.
      data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      int status = 0;
      int sub_status = 0;
      QString error;
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status"_L1) && json_obj.contains("userMessage"_L1)) {
          status = json_obj["status"_L1].toInt();
          sub_status = json_obj["subStatus"_L1].toInt();
          QString user_message = json_obj["userMessage"_L1].toString();
          error = QStringLiteral("%1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
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
      if (status == 401 && sub_status == 6001) {  // User does not have a valid session
        service_->Logout();
      }
      Error(error);
    }
    return QByteArray();
  }

  return data;

}

void TidalCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

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

  if (!json_obj.contains("items"_L1)) {
    Error(QStringLiteral("Json object is missing items."), json_obj);
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }
  QJsonValue value_items = json_obj["items"_L1];

  if (!value_items.isArray()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }
  const QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  CoverProviderSearchResults results;
  int i = 0;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      Error(QStringLiteral("Invalid Json reply, items array item is not a object."));
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (!obj_item.contains("artist"_L1)) {
      Error(QStringLiteral("Invalid Json reply, items array item is missing artist."), obj_item);
      continue;
    }
    QJsonValue value_artist = obj_item["artist"_L1];
    if (!value_artist.isObject()) {
      Error(QStringLiteral("Invalid Json reply, items array item artist is not a object."), value_artist);
      continue;
    }
    QJsonObject obj_artist = value_artist.toObject();
    if (!obj_artist.contains("name"_L1)) {
      Error(QStringLiteral("Invalid Json reply, items array item artist is missing name."), obj_artist);
      continue;
    }
    QString artist = obj_artist["name"_L1].toString();

    QJsonObject obj_album;
    if (obj_item.contains("album"_L1)) {
      QJsonValue value_album = obj_item["album"_L1];
      if (value_album.isObject()) {
        obj_album = value_album.toObject();
      }
      else {
        Error(QStringLiteral("Invalid Json reply, items array item album is not a object."), value_album);
        continue;
      }
    }
    else {
      obj_album = obj_item;
    }

    if (!obj_album.contains("title"_L1) || !obj_album.contains("cover"_L1)) {
      Error(QStringLiteral("Invalid Json reply, items array item album is missing title or cover."), obj_album);
      continue;
    }
    QString album = obj_album["title"_L1].toString();
    QString cover = obj_album["cover"_L1].toString().replace("-"_L1, "/"_L1);

    CoverProviderSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = Song::AlbumRemoveDiscMisc(album);
    cover_result.number = ++i;

    const QList<QPair<QString, QSize>> cover_sizes = QList<QPair<QString, QSize>>() << qMakePair(QStringLiteral("1280x1280"), QSize(1280, 1280))
                                                                                    << qMakePair(QStringLiteral("750x750"), QSize(750, 750))
                                                                                    << qMakePair(QStringLiteral("640x640"), QSize(640, 640));
    for (const QPair<QString, QSize> &cover_size : cover_sizes) {
      QUrl cover_url(QStringLiteral("%1/images/%2/%3.jpg").arg(QLatin1String(TidalService::kResourcesUrl), cover, cover_size.first));
      cover_result.image_url = cover_url;
      cover_result.image_size = cover_size.second;
      results << cover_result;
    }

  }
  Q_EMIT SearchFinished(id, results);

}

void TidalCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
