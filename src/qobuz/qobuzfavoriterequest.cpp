/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QNetworkReply>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"
#include "qobuzfavoriterequest.h"

using namespace Qt::Literals::StringLiterals;

QobuzFavoriteRequest::QobuzFavoriteRequest(QobuzService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QobuzBaseRequest(service, network, parent) {}

QString QobuzFavoriteRequest::FavoriteText(const FavoriteType type) {

  switch (type) {
    case FavoriteType::Artists:
      return u"artists"_s;
    case FavoriteType::Albums:
      return u"albums"_s;
    case FavoriteType::Songs:
    default:
      return u"tracks"_s;
  }

}

QString QobuzFavoriteRequest::FavoriteMethod(const FavoriteType type) {

  switch (type) {
    case FavoriteType::Artists:
      return u"artist_ids"_s;
      break;
    case FavoriteType::Albums:
      return u"album_ids"_s;
      break;
    case FavoriteType::Songs:
      return u"track_ids"_s;
      break;
  }

  return QString();

}

void QobuzFavoriteRequest::AddArtists(const SongList &songs) {
  AddFavorites(FavoriteType::Artists, songs);
}

void QobuzFavoriteRequest::AddAlbums(const SongList &songs) {
  AddFavorites(FavoriteType::Albums, songs);
}

void QobuzFavoriteRequest::AddSongs(const SongList &songs) {
  AddFavorites(FavoriteType::Songs, songs);
}

void QobuzFavoriteRequest::AddSongs(const SongMap &songs) {
  AddFavoritesRequest(FavoriteType::Songs, songs.keys(), songs.values());
}

void QobuzFavoriteRequest::AddFavorites(const FavoriteType type, const SongList &songs) {

  QStringList ids_list;
  for (const Song &song : songs) {
    QString id;
    switch (type) {
      case FavoriteType::Artists:
        if (song.artist_id().isEmpty()) continue;
        id = song.artist_id();
        break;
      case FavoriteType::Albums:
        if (song.album_id().isEmpty()) continue;
        id = song.album_id();
        break;
      case FavoriteType::Songs:
        if (song.song_id().isEmpty()) continue;
        id = song.song_id();
        break;
    }
    if (!id.isEmpty() && !ids_list.contains(id)) {
      ids_list << id;
    }
  }

  if (ids_list.isEmpty()) return;

  AddFavoritesRequest(type, ids_list, songs);

}

void QobuzFavoriteRequest::AddFavoritesRequest(const FavoriteType type, const QStringList &ids_list, const SongList &songs) {

  const ParamList params = ParamList() << Param(u"app_id"_s, service_->app_id())
                                       << Param(u"user_auth_token"_s, service_->user_auth_token())
                                       << Param(FavoriteMethod(type), ids_list.join(u','));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QNetworkReply *reply = CreateRequest(u"favorite/create"_s, params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { AddFavoritesReply(reply, type, songs); });
  replies_ << reply;

}

void QobuzFavoriteRequest::AddFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
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

  qLog(Debug) << "Qobuz:" << songs.count() << "songs added to" << FavoriteText(type) << "favorites.";

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

void QobuzFavoriteRequest::RemoveArtists(const SongList &songs) {
  RemoveFavorites(FavoriteType::Artists, songs);
}

void QobuzFavoriteRequest::RemoveAlbums(const SongList &songs) {
  RemoveFavorites(FavoriteType::Albums, songs);
}

void QobuzFavoriteRequest::RemoveSongs(const SongList &songs) {
  RemoveFavorites(FavoriteType::Songs, songs);
}

void QobuzFavoriteRequest::RemoveSongs(const SongMap &songs) {
  RemoveFavoritesRequest(FavoriteType::Songs, songs.keys(), songs.values());
}

void QobuzFavoriteRequest::RemoveFavorites(const FavoriteType type, const SongList &songs) {

  QStringList ids_list;
  for (const Song &song : songs) {
    QString id;
    switch (type) {
      case FavoriteType::Artists:
        if (song.artist_id().isEmpty()) continue;
        id = song.artist_id();
        break;
      case FavoriteType::Albums:
        if (song.album_id().isEmpty()) continue;
        id = song.album_id();
        break;
      case FavoriteType::Songs:
        if (song.song_id().isEmpty()) continue;
        id = song.song_id();
        break;
    }
    if (!id.isEmpty() && !ids_list.contains(id)) {
      ids_list << id;
    }
  }

  if (ids_list.isEmpty()) return;

  RemoveFavoritesRequest(type, ids_list, songs);

}

void QobuzFavoriteRequest::RemoveFavoritesRequest(const FavoriteType type, const QStringList &ids_list, const SongList &songs) {

  const ParamList params = ParamList() << Param(u"app_id"_s, service_->app_id())
                                       << Param(u"user_auth_token"_s, service_->user_auth_token())
                                       << Param(FavoriteMethod(type), ids_list.join(u','));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QNetworkReply *reply = CreateRequest(u"favorite/delete"_s, params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { RemoveFavoritesReply(reply, type, songs); });
  replies_ << reply;

}

void QobuzFavoriteRequest::RemoveFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
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

  qLog(Debug) << "Qobuz:" << songs.count() << "songs removed from" << FavoriteText(type) << "favorites.";

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

void QobuzFavoriteRequest::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Qobuz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
