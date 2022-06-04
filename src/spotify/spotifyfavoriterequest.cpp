/*
 * Strawberry Music Player
 * Copyright 2022, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QPair>
#include <QList>
#include <QMap>
#include <QMultiMap>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "spotifyservice.h"
#include "spotifybaserequest.h"
#include "spotifyfavoriterequest.h"

SpotifyFavoriteRequest::SpotifyFavoriteRequest(SpotifyService *service, NetworkAccessManager *network, QObject *parent)
    : SpotifyBaseRequest(service, network, parent),
      service_(service),
      network_(network) {}

SpotifyFavoriteRequest::~SpotifyFavoriteRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

QString SpotifyFavoriteRequest::FavoriteText(const FavoriteType type) {

  switch (type) {
    case FavoriteType_Artists:
      return "artists";
    case FavoriteType_Albums:
      return "albums";
    case FavoriteType_Songs:
      return "tracks";
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

  QByteArray json_data = QJsonDocument(array_ids).toJson();
  QString ids_list = list_ids.join(",");

  AddFavoritesRequest(type, ids_list, json_data, songs);

}

void SpotifyFavoriteRequest::AddFavoritesRequest(const FavoriteType type, const QString &ids_list, const QByteArray &json_data, const SongList &songs) {

  QUrl url(SpotifyService::kApiUrl + (type == FavoriteType_Artists ? QString("/me/following") : QString("/me/") + FavoriteText(type)));
  if (type == FavoriteType_Artists) {
    QUrlQuery url_query;
    url_query.addQueryItem("type", "artist");
    url_query.addQueryItem("ids", ids_list);
    url.setQuery(url_query);
  }
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  if (!access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + access_token().toUtf8());
  QNetworkReply *reply = nullptr;
  if (type == FavoriteType_Artists) {
    reply = network_->put(req, "");
  }
  else {
    reply = network_->put(req, json_data);
  }
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { AddFavoritesReply(reply, type, songs); });
  replies_ << reply;

}

void SpotifyFavoriteRequest::AddFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  GetReplyData(reply);
  if (reply->error() != QNetworkReply::NoError) {
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

  QByteArray json_data = QJsonDocument(array_ids).toJson();
  QString ids_list = list_ids.join(",");

  RemoveFavoritesRequest(type, ids_list, json_data, songs);

}

void SpotifyFavoriteRequest::RemoveFavoritesRequest(const FavoriteType type, const QString &ids_list, const QByteArray &json_data, const SongList &songs) {

  Q_UNUSED(json_data)

  QUrl url(SpotifyService::kApiUrl + (type == FavoriteType_Artists ? QString("/me/following") : QString("/me/") + FavoriteText(type)));
  if (type == FavoriteType_Artists) {
    QUrlQuery url_query;
    url_query.addQueryItem("type", "artist");
    url_query.addQueryItem("ids", ids_list);
    url.setQuery(url_query);
  }
  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  if (!access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + access_token().toUtf8());
  QNetworkReply *reply = nullptr;
  if (type == FavoriteType_Artists) {
    reply = network_->deleteResource(req);
  }
  else {
    // FIXME
    reply = network_->deleteResource(req);
  }
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { RemoveFavoritesReply(reply, type, songs); });
  replies_ << reply;

}

void SpotifyFavoriteRequest::RemoveFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  GetReplyData(reply);
  if (reply->error() != QNetworkReply::NoError) {
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

void SpotifyFavoriteRequest::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Spotify:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
