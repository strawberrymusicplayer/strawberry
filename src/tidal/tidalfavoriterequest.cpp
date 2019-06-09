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
#include "tidalservice.h"
#include "tidalbaserequest.h"
#include "tidalfavoriterequest.h"

TidalFavoriteRequest::TidalFavoriteRequest(TidalService *service, NetworkAccessManager *network, QObject *parent)
    : TidalBaseRequest(service, network, parent),
    service_(service),
    network_(network),
    need_login_(false) {}

TidalFavoriteRequest::~TidalFavoriteRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, 0, nullptr, 0);
    reply->abort();
    reply->deleteLater();
  }

}

QString TidalFavoriteRequest::FavoriteText(const FavoriteType type) {

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

void TidalFavoriteRequest::AddArtists(const SongList &songs) {
  AddFavorites(FavoriteType_Artists, songs);
}

void TidalFavoriteRequest::AddAlbums(const SongList &songs) {
  AddFavorites(FavoriteType_Albums, songs);
}

void TidalFavoriteRequest::AddSongs(const SongList &songs) {
  AddFavorites(FavoriteType_Songs, songs);
}

void TidalFavoriteRequest::AddFavorites(const FavoriteType type, const SongList &songs) {

  if (songs.isEmpty()) return;

  QString text;
  switch (type) {
    case FavoriteType_Artists:
      text = "artistIds";
      break;
  case FavoriteType_Albums:
      text = "albumIds";
      break;
  case FavoriteType_Songs:
      text = "trackIds";
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
        if (song.album_id() <= 0) continue;
        id = QString::number(song.album_id());
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
  typedef QList<EncodedParam> EncodedParamList;

  ParamList params = ParamList() << Param("countryCode", country_code())
                                 << Param(text, ids);

  QStringList query_items;
  QUrlQuery url_query;
  for (const Param& param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    query_items << QString(encoded_param.first + "=" + encoded_param.second);
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(api_url() + QString("/") + "users/" + QString::number(service_->user_id()) + "/favorites/" + FavoriteText(type));
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  if (!access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + access_token().toUtf8());
  if (!session_id().isEmpty()) req.setRawHeader("X-Tidal-SessionId", session_id().toUtf8());
  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AddFavoritesReply(QNetworkReply*, const FavoriteType, const SongList&)), reply, type, songs);
  replies_ << reply;

  qLog(Debug) << "Tidal: Sending request" << url << query;

}

void TidalFavoriteRequest::AddFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    reply->deleteLater();
  }
  else {
    return;
  }

  QString error;
  QByteArray data = GetReplyData(reply, error, false);

  if (reply->error() != QNetworkReply::NoError) {
    return;
  }

  qLog(Debug) << "Tidal:" << songs.count() << "songs added to" << FavoriteText(type) << "favorites.";

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

void TidalFavoriteRequest::RemoveArtists(const SongList &songs) {
  RemoveFavorites(FavoriteType_Artists, songs);
}

void TidalFavoriteRequest::RemoveAlbums(const SongList &songs) {
  RemoveFavorites(FavoriteType_Albums, songs);
}

void TidalFavoriteRequest::RemoveSongs(const SongList &songs) {
  RemoveFavorites(FavoriteType_Songs, songs);
}

void TidalFavoriteRequest::RemoveFavorites(const FavoriteType type, const SongList songs) {

  if (songs.isEmpty()) return;

  QList<int> ids;
  QMultiMap<int, Song> songs_map;
  for (const Song &song : songs) {
    int id = -1;
    switch (type) {
      case FavoriteType_Artists:
        if (song.artist_id() <= 0) continue;
        id = song.artist_id();
        break;
    case FavoriteType_Albums:
        if (song.album_id() <= 0) continue;
        id = song.album_id();
        break;
    case FavoriteType_Songs:
        if (song.song_id() <= 0) continue;
        id = song.song_id();
        break;
    }
    if (!ids.contains(id)) ids << id;
    songs_map.insertMulti(id, song);
  }

  for (int id : ids) {
    SongList songs_list = songs_map.values(id);
    RemoveFavorites(type, id, songs_list);
  }

}

void TidalFavoriteRequest::RemoveFavorites(const FavoriteType type, const int id, const SongList &songs) {

  ParamList params = ParamList() << Param("countryCode", country_code());

  QStringList query_items;
  QUrlQuery url_query;
  for (const Param& param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    query_items << QString(encoded_param.first + "=" + encoded_param.second);
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(api_url() + QString("/") + "users/" + QString::number(service_->user_id()) + "/favorites/" + FavoriteText(type) + QString("/") + QString::number(id));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  if (!access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + access_token().toUtf8());
  if (!session_id().isEmpty()) req.setRawHeader("X-Tidal-SessionId", session_id().toUtf8());
  QNetworkReply *reply = network_->deleteResource(req);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(RemoveFavoritesReply(QNetworkReply*, const FavoriteType, const SongList&)), reply, type, songs);
  replies_ << reply;

  qLog(Debug) << "Tidal: Sending request" << url << "with" << songs.count() << "songs";

}

void TidalFavoriteRequest::RemoveFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    reply->deleteLater();
  }
  else {
    return;
  }

  QString error;
  QByteArray data = GetReplyData(reply, error, false);
  if (reply->error() != QNetworkReply::NoError) {
    return;
  }

  qLog(Debug) << "Tidal:" << songs.count() << "songs removed from" << FavoriteText(type) << "favorites.";

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
