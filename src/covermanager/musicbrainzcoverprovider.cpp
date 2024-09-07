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

#include <algorithm>

#include <QtGlobal>
#include <QObject>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
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
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "musicbrainzcoverprovider.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kReleaseSearchUrl[] = "https://musicbrainz.org/ws/2/release/";
constexpr char kAlbumCoverUrl[] = "https://coverartarchive.org/release/%1/front";
constexpr int kLimit = 8;
constexpr int kRequestsDelay = 1000;
}  // namespace

MusicbrainzCoverProvider::MusicbrainzCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(QStringLiteral("MusicBrainz"), true, false, 1.5, true, false, app, network, parent),
      timer_flush_requests_(new QTimer(this)) {

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &MusicbrainzCoverProvider::FlushRequests);

}

MusicbrainzCoverProvider::~MusicbrainzCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool MusicbrainzCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  Q_UNUSED(title);

  if (artist.isEmpty() || album.isEmpty()) return false;

  SearchRequest request(id, artist, album);
  queue_search_requests_ << request;

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

  return true;

}

void MusicbrainzCoverProvider::SendSearchRequest(const SearchRequest &request) {

  QString query = QStringLiteral("release:\"%1\" AND artist:\"%2\"").arg(request.album.trimmed().replace(u'"', "\""_L1), request.artist.trimmed().replace(u'"', "\""_L1));

  QUrlQuery url_query;
  url_query.addQueryItem(QStringLiteral("query"), query);
  url_query.addQueryItem(QStringLiteral("limit"), QString::number(kLimit));
  url_query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));

  QUrl url(QString::fromLatin1(kReleaseSearchUrl));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { HandleSearchReply(reply, request.id); });

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
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverProviderSearchResults results;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    Q_EMIT SearchFinished(search_id, results);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    Q_EMIT SearchFinished(search_id, results);
    return;
  }

  if (!json_obj.contains("releases"_L1)) {
    if (json_obj.contains("error"_L1)) {
      QString error = json_obj["error"_L1].toString();
      Error(error);
    }
    else {
      Error(QStringLiteral("Json reply is missing releases."), json_obj);
    }
    Q_EMIT SearchFinished(search_id, results);
    return;
  }
  QJsonValue value_releases = json_obj["releases"_L1];

  if (!value_releases.isArray()) {
    Error(QStringLiteral("Json releases is not an array."), value_releases);
    Q_EMIT SearchFinished(search_id, results);
    return;
  }
  const QJsonArray array_releases = value_releases.toArray();

  if (array_releases.isEmpty()) {
    Q_EMIT SearchFinished(search_id, results);
    return;
  }

  for (const QJsonValue &value_release : array_releases) {

    if (!value_release.isObject()) {
      Error(QStringLiteral("Invalid Json reply, releases array value is not an object."));
      continue;
    }
    QJsonObject obj_release = value_release.toObject();
    if (!obj_release.contains("id"_L1) || !obj_release.contains("artist-credit"_L1) || !obj_release.contains("title"_L1)) {
      Error(QStringLiteral("Invalid Json reply, releases array object is missing id, artist-credit or title."), obj_release);
      continue;
    }

    QJsonValue json_artists = obj_release["artist-credit"_L1];
    if (!json_artists.isArray()) {
      Error(QStringLiteral("Invalid Json reply, artist-credit is not a array."), json_artists);
      continue;
    }
    const QJsonArray array_artists = json_artists.toArray();
    int i = 0;
    QString artist;
    for (const QJsonValue &value_artist : array_artists) {
      if (!value_artist.isObject()) {
        Error(QStringLiteral("Invalid Json reply, artist is not a object."));
        continue;
      }
      QJsonObject obj_artist = value_artist.toObject();

      if (!obj_artist.contains("artist"_L1)) {
        Error(QStringLiteral("Invalid Json reply, artist is missing."), obj_artist);
        continue;
      }
      QJsonValue value_artist2 = obj_artist["artist"_L1];
      if (!value_artist2.isObject()) {
        Error(QStringLiteral("Invalid Json reply, artist is not an object."), value_artist2);
        continue;
      }
      QJsonObject obj_artist2 = value_artist2.toObject();

      if (!obj_artist2.contains("name"_L1)) {
        Error(QStringLiteral("Invalid Json reply, artist is missing name."), value_artist2);
        continue;
      }
      artist = obj_artist2["name"_L1].toString();
      ++i;
    }
    if (i > 1) artist = "Various artists"_L1;

    QString id = obj_release["id"_L1].toString();
    QString album = obj_release["title"_L1].toString();

    CoverProviderSearchResult cover_result;
    QUrl url(QString::fromLatin1(kAlbumCoverUrl).arg(id));
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = url;
    results.append(cover_result);
  }
  Q_EMIT SearchFinished(search_id, results);

}

QByteArray MusicbrainzCoverProvider::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(failure_reason);
    }
    else {
      // See if there is Json data containing "error" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error"_L1)) {
          error = json_obj["error"_L1].toString();
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

void MusicbrainzCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Musicbrainz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
