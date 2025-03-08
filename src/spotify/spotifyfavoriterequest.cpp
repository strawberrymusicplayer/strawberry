/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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
    : SpotifyBaseRequest(service, network, parent) {}

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

  QStringList list_ids;
  QJsonArray array_ids;
  for (const Song &song : songs) {
    QString id;
    switch (type) {
      case FavoriteType_Artists:
        id = song.artist_id();
        break;
      case FavoriteType_Albums:
        id = song.album_id();
        break;
      case FavoriteType_Songs:
        id = song.song_id();
        break;
    }
    if (!id.isEmpty()) {
      if (!list_ids.contains(id)) {
        list_ids << id;
      }
      if (!array_ids.contains(id)) {
        array_ids << id;
      }
    }
  }

  if (list_ids.isEmpty() || array_ids.isEmpty()) return;

  const QByteArray json_data = QJsonDocument(array_ids).toJson();
  const QString ids_list = list_ids.join(u',');

  AddFavoritesRequest(type, ids_list, json_data, songs);

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

  QStringList list_ids;
  QJsonArray array_ids;
  for (const Song &song : songs) {
    QString id;
    switch (type) {
      case FavoriteType_Artists:
        id = song.artist_id();
        break;
      case FavoriteType_Albums:
        id = song.album_id();
        break;
      case FavoriteType_Songs:
        id = song.song_id();
        break;
    }
    if (!id.isEmpty()) {
      if (!list_ids.contains(id)) {
        list_ids << id;
      }
      if (!array_ids.contains(id)) {
        array_ids << id;
      }
    }
  }

  if (list_ids.isEmpty() || array_ids.isEmpty()) return;

  const QByteArray json_data = QJsonDocument(array_ids).toJson();
  const QString ids_list = list_ids.join(u',');

  RemoveFavoritesRequest(type, ids_list, json_data, songs);

}

void SpotifyFavoriteRequest::RemoveFavoritesRequest(const FavoriteType type, const QString &ids_list, const QByteArray &json_data, const SongList &songs) {

  Q_UNUSED(json_data)

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
    reply = network_->deleteResource(network_request);
  }
  else {
    // FIXME
    reply = network_->deleteResource(network_request);
  }
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { RemoveFavoritesReply(reply, type, songs); });
  replies_ << reply;

}

void SpotifyFavoriteRequest::RemoveFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (!replies_.contains(reply)) return;
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
