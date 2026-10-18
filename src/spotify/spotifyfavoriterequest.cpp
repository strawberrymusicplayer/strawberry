/*
 * Strawberry Music Player
 * Copyright 2022-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QQueue>
#include <QScopeGuard>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "spotifyservice.h"
#include "spotifybaserequest.h"
#include "spotifyfavoriterequest.h"

using namespace Qt::Literals::StringLiterals;

SpotifyFavoriteRequest::SpotifyFavoriteRequest(SpotifyService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : SpotifyBaseRequest(service, network, parent),
      request_active_(false) {}

QString SpotifyFavoriteRequest::FavoriteText(const FavoriteType type) {

  switch (type) {
    case FavoriteType_Artists:
      return u"artists"_s;
    case FavoriteType_Albums:
      return u"albums"_s;
    case FavoriteType_Songs:
      return u"tracks"_s;
  }

  return QString();

}

qsizetype SpotifyFavoriteRequest::MaxIds(const FavoriteType type) {

  // The maximum number of IDs the API accepts in one request.
  switch (type) {
    case FavoriteType_Artists:
      return 50;
    case FavoriteType_Albums:
      return 20;
    case FavoriteType_Songs:
      return 50;
  }

  return 20;

}

QString SpotifyFavoriteRequest::FavoriteId(const FavoriteType type, const Song &song) {

  switch (type) {
    case FavoriteType_Artists:
      return song.artist_id();
    case FavoriteType_Albums:
      return song.album_id();
    case FavoriteType_Songs:
      return song.song_id();
  }

  return QString();

}

QList<SpotifyFavoriteRequest::FavoriteBatch> SpotifyFavoriteRequest::SplitFavorites(const FavoriteType type, const SongList &songs) {

  // Split the unique IDs into batches the API accepts, with the songs for the IDs in each batch, so each reply only reports the songs it added or removed.
  QList<FavoriteBatch> batches;
  QMap<QString, qsizetype> batch_for_id;
  for (const Song &song : songs) {
    const QString id = FavoriteId(type, song);
    if (id.isEmpty()) continue;
    if (!batch_for_id.contains(id)) {
      if (batches.isEmpty() || batches.last().ids.count() >= MaxIds(type)) {
        batches << FavoriteBatch();
      }
      batches.last().ids << id;
      batch_for_id.insert(id, batches.count() - 1);
    }
    batches[batch_for_id.value(id)].songs << song;
  }

  return batches;

}

void SpotifyFavoriteRequest::FlushRequests() {

  // The batches are sent one at a time, so a large selection does not send all requests at once.
  if (request_active_ || requests_queue_.isEmpty()) return;

  const QueuedRequest request = requests_queue_.dequeue();
  const QByteArray json_data = QJsonDocument(QJsonArray::fromStringList(request.batch.ids)).toJson();
  request_active_ = true;
  if (request.remove) {
    RemoveFavoritesRequest(request.type, request.batch.ids.join(u','), json_data, request.batch.songs);
  }
  else {
    AddFavoritesRequest(request.type, request.batch.ids.join(u','), json_data, request.batch.songs);
  }

}

void SpotifyFavoriteRequest::AddArtists(const SongList &songs) {
  AddFavorites(FavoriteType_Artists, songs);
}

void SpotifyFavoriteRequest::AddAlbums(const SongList &songs) {
  AddFavorites(FavoriteType_Albums, songs);
}

void SpotifyFavoriteRequest::AddSongs(const SongList &songs) {
  AddFavorites(FavoriteType_Songs, songs);
}

void SpotifyFavoriteRequest::AddSongs(const SongMap &songs) {
  AddFavorites(FavoriteType_Songs, songs.values());
}

void SpotifyFavoriteRequest::AddFavorites(const FavoriteType type, const SongList &songs) {

  const QList<FavoriteBatch> batches = SplitFavorites(type, songs);
  for (const FavoriteBatch &batch : batches) {
    requests_queue_.enqueue(QueuedRequest{false, type, batch});
  }

  FlushRequests();

}

void SpotifyFavoriteRequest::AddFavoritesRequest(const FavoriteType type, const QString &ids_list, const QByteArray &json_data, const SongList &songs) {

  QUrl url(QLatin1String(SpotifyService::kApiUrl) + (type == FavoriteType_Artists ? u"/me/following"_s : u"/me/"_s + FavoriteText(type)));
  if (type == FavoriteType_Artists) {
    QUrlQuery url_query;
    url_query.addQueryItem(u"type"_s, u"artist"_s);
    url_query.addQueryItem(u"ids"_s, ids_list);
    url.setQuery(url_query);
  }
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  if (service_->authenticated()) {
    network_request.setRawHeader("Authorization", service_->authorization_header());
  }
  QNetworkReply *reply = nullptr;
  if (type == FavoriteType_Artists) {
    reply = network_->put(network_request, "");
  }
  else {
    reply = network_->put(network_request, json_data);
  }
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { AddFavoritesReply(reply, type, songs); });
  replies_ << reply;

}

void SpotifyFavoriteRequest::AddFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (!replies_.contains(reply)) return;

  // Send the next batch when this one is finished, also when it failed.
  const QScopeGuard send_next_request = qScopeGuard([this]() {
    request_active_ = false;
    FlushRequests();
  });
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  if (type == FavoriteType_Artists) {
    qLog(Debug) << "Spotify:" << songs.count() << "songs added to followed" << FavoriteText(type);
  }
  else {
    qLog(Debug) << "Spotify:" << songs.count() << "songs added to saved" << FavoriteText(type);
  }

  switch (type) {
    case FavoriteType_Artists:
      Q_EMIT ArtistsAdded(songs);
      break;
    case FavoriteType_Albums:
      Q_EMIT AlbumsAdded(songs);
      break;
    case FavoriteType_Songs:
      Q_EMIT SongsAdded(songs);
      break;
  }

}

void SpotifyFavoriteRequest::RemoveArtists(const SongList &songs) {
  RemoveFavorites(FavoriteType_Artists, songs);
}

void SpotifyFavoriteRequest::RemoveAlbums(const SongList &songs) {
  RemoveFavorites(FavoriteType_Albums, songs);
}

void SpotifyFavoriteRequest::RemoveSongs(const SongList &songs) {
  RemoveFavorites(FavoriteType_Songs, songs);
}

void SpotifyFavoriteRequest::RemoveSongs(const SongMap &songs) {

  RemoveFavorites(FavoriteType_Songs, songs.values());

}

void SpotifyFavoriteRequest::RemoveFavorites(const FavoriteType type, const SongList &songs) {

  const QList<FavoriteBatch> batches = SplitFavorites(type, songs);
  for (const FavoriteBatch &batch : batches) {
    requests_queue_.enqueue(QueuedRequest{true, type, batch});
  }

  FlushRequests();

}

void SpotifyFavoriteRequest::RemoveFavoritesRequest(const FavoriteType type, const QString &ids_list, const QByteArray &json_data, const SongList &songs) {

  Q_UNUSED(json_data)

  QUrl url(QLatin1String(SpotifyService::kApiUrl) + (type == FavoriteType_Artists ? u"/me/following"_s : u"/me/"_s + FavoriteText(type)));
  QUrlQuery url_query;
  if (type == FavoriteType_Artists) {
    url_query.addQueryItem(u"type"_s, u"artist"_s);
  }
  url_query.addQueryItem(u"ids"_s, ids_list);
  url.setQuery(url_query);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  if (service_->authenticated()) {
    network_request.setRawHeader("Authorization", service_->authorization_header());
  }
  QNetworkReply *reply = network_->deleteResource(network_request);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { RemoveFavoritesReply(reply, type, songs); });
  replies_ << reply;

}

void SpotifyFavoriteRequest::RemoveFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (!replies_.contains(reply)) return;

  // Send the next batch when this one is finished, also when it failed.
  const QScopeGuard send_next_request = qScopeGuard([this]() {
    request_active_ = false;
    FlushRequests();
  });
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  if (type == FavoriteType_Artists) {
    qLog(Debug) << "Spotify:" << songs.count() << "songs removed from followed" << FavoriteText(type);
  }
  else {
    qLog(Debug) << "Spotify:" << songs.count() << "songs removed from saved" << FavoriteText(type);
  }

  switch (type) {
    case FavoriteType_Artists:
      Q_EMIT ArtistsRemoved(songs);
      break;
    case FavoriteType_Albums:
      Q_EMIT AlbumsRemoved(songs);
      break;
    case FavoriteType_Songs:
      Q_EMIT SongsRemoved(songs);
      break;
  }

}
