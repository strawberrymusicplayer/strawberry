/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QPair>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"
#include "qobuzfavoriterequest.h"

QobuzFavoriteRequest::QobuzFavoriteRequest(QobuzService *service, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QobuzBaseRequest(service, network, parent),
      service_(service),
      network_(network) {}

QobuzFavoriteRequest::~QobuzFavoriteRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

QString QobuzFavoriteRequest::FavoriteText(const FavoriteType type) {

  switch (type) {
    case FavoriteType::Artists:
      return QStringLiteral("artists");
    case FavoriteType::Albums:
      return QStringLiteral("albums");
    case FavoriteType::Songs:
    default:
      return QStringLiteral("tracks");
  }

}

QString QobuzFavoriteRequest::FavoriteMethod(const FavoriteType type) {

  switch (type) {
    case FavoriteType::Artists:
      return QStringLiteral("artist_ids");
      break;
    case FavoriteType::Albums:
      return QStringLiteral("album_ids");
      break;
    case FavoriteType::Songs:
      return QStringLiteral("track_ids");
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

  const ParamList params = ParamList() << Param(QStringLiteral("app_id"), app_id())
                                       << Param(QStringLiteral("user_auth_token"), user_auth_token())
                                       << Param(FavoriteMethod(type), ids_list.join(u','));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QNetworkReply *reply = CreateRequest(QStringLiteral("favorite/create"), params);
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

  GetReplyData(reply);

  if (reply->error() != QNetworkReply::NoError) {
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

  const ParamList params = ParamList() << Param(QStringLiteral("app_id"), app_id())
                                       << Param(QStringLiteral("user_auth_token"), user_auth_token())
                                       << Param(FavoriteMethod(type), ids_list.join(u','));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QNetworkReply *reply = CreateRequest(QStringLiteral("favorite/delete"), params);
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

  GetReplyData(reply);
  if (reply->error() != QNetworkReply::NoError) {
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
