/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QMultiMap>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "opentidalservice.h"
#include "opentidalbaserequest.h"
#include "opentidalfavoriterequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kContentType[] = "application/vnd.api+json";
}  // namespace

OpenTidalFavoriteRequest::OpenTidalFavoriteRequest(OpenTidalService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : OpenTidalBaseRequest(service, network, parent),
      service_(service),
      network_(network) {}

QString OpenTidalFavoriteRequest::FavoriteText(const FavoriteType type) {

  switch (type) {
    case FavoriteType::Artists:
      return u"artists"_s;
    case FavoriteType::Albums:
      return u"albums"_s;
    case FavoriteType::Songs:
      return u"tracks"_s;
  }

  return QString();

}

QString OpenTidalFavoriteRequest::FavoriteMethod(const FavoriteType type) {

  // The JSON:API resource type matches the relationship name.
  return FavoriteText(type);

}

void OpenTidalFavoriteRequest::AddArtists(const SongList &songs) {
  AddFavorites(FavoriteType::Artists, songs);
}

void OpenTidalFavoriteRequest::AddAlbums(const SongList &songs) {
  AddFavorites(FavoriteType::Albums, songs);
}

void OpenTidalFavoriteRequest::AddSongs(const SongList &songs) {
  AddFavorites(FavoriteType::Songs, songs);
}

void OpenTidalFavoriteRequest::AddSongs(const SongMap &songs) {
  AddFavoritesRequest(FavoriteType::Songs, songs.keys(), songs.values());
}

void OpenTidalFavoriteRequest::AddFavorites(const FavoriteType type, const SongList &songs) {

  QStringList id_list;
  for (const Song &song : songs) {
    QString id;
    switch (type) {
      case FavoriteType::Artists:
        id = song.artist_id();
        break;
      case FavoriteType::Albums:
        id = song.album_id();
        break;
      case FavoriteType::Songs:
        id = song.song_id();
        break;
    }
    if (!id.isEmpty() && !id_list.contains(id)) {
      id_list << id;
    }
  }

  if (id_list.isEmpty()) return;

  AddFavoritesRequest(type, id_list, songs);

}

void OpenTidalFavoriteRequest::AddFavoritesRequest(const FavoriteType type, const QStringList &id_list, const SongList &songs) {

  const QString resource_type = FavoriteText(type);

  QJsonArray array_data;
  for (const QString &id : id_list) {
    QJsonObject object_item;
    object_item.insert("type"_L1, resource_type);
    object_item.insert("id"_L1, id);
    array_data.append(object_item);
  }
  QJsonObject object_body;
  object_body.insert("data"_L1, array_data);
  const QByteArray data = QJsonDocument(object_body).toJson(QJsonDocument::Compact);

  const QUrl url(QLatin1String(OpenTidalService::kApiUrl) + "/userCollections/"_L1 + QString::number(service_->user_id()) + "/relationships/"_L1 + resource_type);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String(kContentType));
  if (authenticated()) {
    network_request.setRawHeader("Authorization", authorization_header());
  }
  QNetworkReply *reply = network_->post(network_request, data);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { AddFavoritesReply(reply, type, songs); });
  replies_ << reply;

  qLog(Debug) << "OpenTidal: Sending request" << url << data;

}

void OpenTidalFavoriteRequest::AddFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();
  }
  else {
    return;
  }

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  qLog(Debug) << "OpenTidal:" << songs.count() << "songs added to" << FavoriteText(type) << "favorites.";

  switch (type) {
    case FavoriteType::Artists:
      Q_EMIT ArtistsAdded(songs);
      break;
    case FavoriteType::Albums:
      Q_EMIT AlbumsAdded(songs);
      break;
    case FavoriteType::Songs:
      Q_EMIT SongsAdded(songs);
      break;
  }

}

void OpenTidalFavoriteRequest::RemoveArtists(const SongList &songs) {
  RemoveFavorites(FavoriteType::Artists, songs);
}

void OpenTidalFavoriteRequest::RemoveAlbums(const SongList &songs) {
  RemoveFavorites(FavoriteType::Albums, songs);
}

void OpenTidalFavoriteRequest::RemoveSongs(const SongList &songs) {
  RemoveFavorites(FavoriteType::Songs, songs);
}

void OpenTidalFavoriteRequest::RemoveSongs(const SongMap &songs) {

  const SongList songs_list = songs.values();
  for (const Song &song : songs_list) {
    RemoveFavoritesRequest(FavoriteType::Songs, song.song_id(), SongList() << song);
  }

}

void OpenTidalFavoriteRequest::RemoveFavorites(const FavoriteType type, const SongList &songs) {

  QMultiMap<QString, Song> songs_map;
  for (const Song &song : songs) {
    QString id;
    switch (type) {
      case FavoriteType::Artists:
        id = song.artist_id();
        break;
      case FavoriteType::Albums:
        id = song.album_id();
        break;
      case FavoriteType::Songs:
        id = song.song_id();
        break;
    }
    if (!id.isEmpty()) {
      songs_map.insert(id, song);
    }
  }

  const QStringList ids = songs_map.uniqueKeys();
  for (const QString &id : ids) {
    RemoveFavoritesRequest(type, id, songs_map.values(id));
  }

}

void OpenTidalFavoriteRequest::RemoveFavoritesRequest(const FavoriteType type, const QString &id, const SongList &songs) {

  if (id.isEmpty()) return;

  const QString resource_type = FavoriteText(type);

  QJsonObject object_item;
  object_item.insert("type"_L1, resource_type);
  object_item.insert("id"_L1, id);
  QJsonArray array_data;
  array_data.append(object_item);
  QJsonObject object_body;
  object_body.insert("data"_L1, array_data);
  const QByteArray data = QJsonDocument(object_body).toJson(QJsonDocument::Compact);

  const QUrl url(QLatin1String(OpenTidalService::kApiUrl) + "/userCollections/"_L1 + QString::number(service_->user_id()) + "/relationships/"_L1 + resource_type);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String(kContentType));
  if (authenticated()) {
    network_request.setRawHeader("Authorization", authorization_header());
  }
  QNetworkReply *reply = network_->sendCustomRequest(network_request, "DELETE", data);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { RemoveFavoritesReply(reply, type, songs); });
  replies_ << reply;

  qLog(Debug) << "OpenTidal: Sending request" << url << "with" << songs.count() << "songs";

}

void OpenTidalFavoriteRequest::RemoveFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();
  }
  else {
    return;
  }

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  qLog(Debug) << "OpenTidal:" << songs.count() << "songs removed from" << FavoriteText(type) << "favorites.";

  switch (type) {
    case FavoriteType::Artists:
      Q_EMIT ArtistsRemoved(songs);
      break;
    case FavoriteType::Albums:
      Q_EMIT AlbumsRemoved(songs);
      break;
    case FavoriteType::Songs:
      Q_EMIT SongsRemoved(songs);
      break;
  }

}

void OpenTidalFavoriteRequest::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "OpenTidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
