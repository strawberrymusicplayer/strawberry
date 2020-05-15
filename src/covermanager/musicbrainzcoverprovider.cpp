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
#include <QQueue>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QtDebug>

#include "core/application.h"
#include "core/network.h"
#include "core/logging.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "musicbrainzcoverprovider.h"

const char *MusicbrainzCoverProvider::kReleaseSearchUrl = "https://musicbrainz.org/ws/2/release/";
const char *MusicbrainzCoverProvider::kAlbumCoverUrl = "https://coverartarchive.org/release/%1/front";
const int MusicbrainzCoverProvider::kLimit = 8;
const int MusicbrainzCoverProvider::kRequestsDelay = 1000;

MusicbrainzCoverProvider::MusicbrainzCoverProvider(Application *app, QObject *parent): JsonCoverProvider("MusicBrainz", true, false, 1.5, true, false, app, parent), network_(new NetworkAccessManager(this)), timer_flush_requests_(new QTimer(this)) {

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  connect(timer_flush_requests_, SIGNAL(timeout()), this, SLOT(FlushRequests()));

}

MusicbrainzCoverProvider::~MusicbrainzCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool MusicbrainzCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  Q_UNUSED(title);

  SearchRequest request(id, artist, album);
  queue_search_requests_ << request;

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

  return true;

}

void MusicbrainzCoverProvider::SendSearchRequest(const SearchRequest &request) {

  QString query = QString("release:\"%1\" AND artist:\"%2\"").arg(request.album.trimmed().replace('"', "\\\"")).arg(request.artist.trimmed().replace('"', "\\\""));

  QUrlQuery url_query;
  url_query.addQueryItem("query", query);
  url_query.addQueryItem("limit", QString::number(kLimit));
  url_query.addQueryItem("fmt", "json");

  QUrl url(kReleaseSearchUrl);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, request.id); });

}

void MusicbrainzCoverProvider::FlushRequests() {

  if (!queue_search_requests_.isEmpty()) {
    SendSearchRequest(queue_search_requests_.dequeue());
    return;
  }

  timer_flush_requests_->stop();

}

void MusicbrainzCoverProvider::HandleSearchReply(QNetworkReply *reply, const int search_id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverSearchResults results;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(search_id, results);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit SearchFinished(search_id, results);
    return;
  }

  if (!json_obj.contains("releases")) {
    if (json_obj.contains("error")) {
      QString error = json_obj["error"].toString();
      Error(error);
    }
    else {
      Error(QString("Json reply is missing releases."), json_obj);
    }
    emit SearchFinished(search_id, results);
    return;
  }
  QJsonValue value_releases = json_obj["releases"];

  if (!value_releases.isArray()) {
    Error("Json releases is not an array.", value_releases);
    emit SearchFinished(search_id, results);
    return;
  }
  QJsonArray array_releases = value_releases.toArray();

  if (array_releases.isEmpty()) {
    emit SearchFinished(search_id, results);
    return;
  }

  for (const QJsonValue &value_release : array_releases) {

    if (!value_release.isObject()) {
      Error("Invalid Json reply, releases array value is not an object.", value_release);
      continue;
    }
    QJsonObject obj_release = value_release.toObject();
    if (!obj_release.contains("id") || !obj_release.contains("artist-credit") ||  !obj_release.contains("title")) {
      Error("Invalid Json reply, releases array object is missing id, artist-credit or title.", obj_release);
      continue;
    }

    QJsonValue json_artists = obj_release["artist-credit"];
    if (!json_artists.isArray()) {
      Error("Invalid Json reply, artist-credit is not a array.", json_artists);
      continue;
    }
    QJsonArray array_artists = json_artists.toArray();
    int i = 0;
    QString artist;
    for (const QJsonValue &value_artist : array_artists) {
      if (!value_artist.isObject()) {
        Error("Invalid Json reply, artist is not a object.", value_artist);
        continue;
      }
      QJsonObject obj_artist = value_artist.toObject();

      if (!obj_artist.contains("artist") ) {
        Error("Invalid Json reply, artist is missing.", obj_artist);
        continue;
      }
      QJsonValue value_artist2 = obj_artist["artist"];
      if (!value_artist2.isObject()) {
        Error("Invalid Json reply, artist is not an object.", value_artist2);
        continue;
      }
      QJsonObject obj_artist2 = value_artist2.toObject();

      if (!obj_artist2.contains("name") ) {
        Error("Invalid Json reply, artist is missing name.", value_artist2);
        continue;
      }
      artist = obj_artist2["name"].toString();
      ++i;
    }
    if (i > 1) artist = "Various artists";

    QString id = obj_release["id"].toString();
    QString album = obj_release["title"].toString();

    CoverSearchResult cover_result;
    QUrl url(QString(kAlbumCoverUrl).arg(id));
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = url;
    results.append(cover_result);
  }
  emit SearchFinished(search_id, results);

}

QByteArray MusicbrainzCoverProvider::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(failure_reason);
    }
    else {
      // See if there is Json data containing "error" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error")) {
          error = json_obj["error"].toString();
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

void MusicbrainzCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Musicbrainz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
