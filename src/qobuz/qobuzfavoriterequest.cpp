/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QList>
#include <QMultiMap>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>

#include "core/logging.h"
#include "core/network.h"
#include "core/closure.h"
#include "core/song.h"
#include "qobuzservice.h"
#include "qobuzbaserequest.h"
#include "qobuzfavoriterequest.h"

QobuzFavoriteRequest::QobuzFavoriteRequest(QobuzService *service, NetworkAccessManager *network, QObject *parent)
    : QobuzBaseRequest(service, network, parent),
    service_(service),
    network_(network) {}

QobuzFavoriteRequest::~QobuzFavoriteRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, 0, nullptr, 0);
    reply->abort();
    reply->deleteLater();
  }

}

QString QobuzFavoriteRequest::FavoriteText(const FavoriteType type) {

  switch (type) {
    case FavoriteType_Artists:
      return "artists";
    case FavoriteType_Albums:
      return "albums";
    case FavoriteType_Songs:
    default:
      return "tracks";
  }

}

void QobuzFavoriteRequest::AddArtists(const SongList &songs) {
  AddFavorites(FavoriteType_Artists, songs);
}

void QobuzFavoriteRequest::AddAlbums(const SongList &songs) {
  AddFavorites(FavoriteType_Albums, songs);
}

void QobuzFavoriteRequest::AddSongs(const SongList &songs) {
  AddFavorites(FavoriteType_Songs, songs);
}

void QobuzFavoriteRequest::AddFavorites(const FavoriteType type, const SongList &songs) {

  if (songs.isEmpty()) return;

  QString text;
  switch (type) {
    case FavoriteType_Artists:
      text = "artist_ids";
      break;
  case FavoriteType_Albums:
      text = "album_ids";
      break;
  case FavoriteType_Songs:
      text = "track_ids";
      break;
  }

  QStringList ids_list;
  for (const Song &song : songs) {
    QString id;
    switch (type) {
      case FavoriteType_Artists:
        if (song.artist_id() <= 0) continue;
        id = QString::number(song.artist_id());
        break;
    case FavoriteType_Albums:
        if (song.album_id().isEmpty()) continue;
        id = song.album_id();
        break;
    case FavoriteType_Songs:
        if (song.song_id() <= 0) continue;
        id = QString::number(song.song_id());
        break;
    }
    if (id.isEmpty()) continue;
    if (!ids_list.contains(id)) {
      ids_list << id;
    }
  }
  if (ids_list.isEmpty()) return;

  QString ids = ids_list.join(',');

  typedef QPair<QByteArray, QByteArray> EncodedParam;

  ParamList params = ParamList() << Param("app_id", app_id())
                                 << Param("user_auth_token", user_auth_token())
                                 << Param(text, ids);

  QUrlQuery url_query;
  for (const Param& param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QNetworkReply *reply = CreateRequest("favorite/create", params);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AddFavoritesReply(QNetworkReply*, const FavoriteType, const SongList&)), reply, type, songs);
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

  QByteArray data = GetReplyData(reply);

  if (reply->error() != QNetworkReply::NoError) {
    return;
  }

  qLog(Debug) << "Qobuz:" << songs.count() << "songs added to" << FavoriteText(type) << "favorites.";

  switch (type) {
    case FavoriteType_Artists:
      emit ArtistsAdded(songs);
      break;
  case FavoriteType_Albums:
      emit AlbumsAdded(songs);
      break;
  case FavoriteType_Songs:
      emit SongsAdded(songs);
      break;
  }

}

void QobuzFavoriteRequest::RemoveArtists(const SongList &songs) {
  RemoveFavorites(FavoriteType_Artists, songs);
}

void QobuzFavoriteRequest::RemoveAlbums(const SongList &songs) {
  RemoveFavorites(FavoriteType_Albums, songs);
}

void QobuzFavoriteRequest::RemoveSongs(const SongList &songs) {
  RemoveFavorites(FavoriteType_Songs, songs);
}

void QobuzFavoriteRequest::RemoveFavorites(const FavoriteType type, const SongList &songs) {

  if (songs.isEmpty()) return;

  QString text;
  switch (type) {
    case FavoriteType_Artists:
      text = "artist_ids";
      break;
  case FavoriteType_Albums:
      text = "album_ids";
      break;
  case FavoriteType_Songs:
      text = "track_ids";
      break;
  }

  QStringList ids_list;
  for (const Song &song : songs) {
    QString id;
    switch (type) {
      case FavoriteType_Artists:
        if (song.artist_id() <= 0) continue;
        id = QString::number(song.artist_id());
        break;
    case FavoriteType_Albums:
        if (song.album_id().isEmpty()) continue;
        id = song.album_id();
        break;
    case FavoriteType_Songs:
        if (song.song_id() <= 0) continue;
        id = QString::number(song.song_id());
        break;
    }
    if (id.isEmpty()) continue;
    if (!ids_list.contains(id)) {
      ids_list << id;
    }
  }
  if (ids_list.isEmpty()) return;

  QString ids = ids_list.join(',');

  ParamList params = ParamList() << Param("app_id", app_id())
                                 << Param("user_auth_token", user_auth_token())
                                 << Param(text, ids);

  QUrlQuery url_query;
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QNetworkReply *reply = CreateRequest("favorite/delete", params);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(RemoveFavoritesReply(QNetworkReply*, const FavoriteType, const SongList&)), reply, type, songs);
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

  QByteArray data = GetReplyData(reply);
  if (reply->error() != QNetworkReply::NoError) {
    return;
  }

  qLog(Debug) << "Qobuz:" << songs.count() << "songs removed from" << FavoriteText(type) << "favorites.";

  switch (type) {
    case FavoriteType_Artists:
      emit ArtistsRemoved(songs);
      break;
  case FavoriteType_Albums:
      emit AlbumsRemoved(songs);
      break;
  case FavoriteType_Songs:
      emit SongsRemoved(songs);
      break;
  }

}

void QobuzFavoriteRequest::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Qobuz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
