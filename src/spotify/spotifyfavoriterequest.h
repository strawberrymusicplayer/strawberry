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

#ifndef SPOTIFYFAVORITEREQUEST_H
#define SPOTIFYFAVORITEREQUEST_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QByteArray>
#include <QString>

#include "spotifybaserequest.h"
#include "core/song.h"

class QNetworkReply;
class SpotifyService;
class NetworkAccessManager;

class SpotifyFavoriteRequest : public SpotifyBaseRequest {
  Q_OBJECT

 public:
  explicit SpotifyFavoriteRequest(SpotifyService *service, NetworkAccessManager *network, QObject *parent = nullptr);
  ~SpotifyFavoriteRequest() override;

  enum FavoriteType {
    FavoriteType_Artists,
    FavoriteType_Albums,
    FavoriteType_Songs
  };

 signals:
  void ArtistsAdded(SongList);
  void AlbumsAdded(SongList);
  void SongsAdded(SongList);
  void ArtistsRemoved(SongList);
  void AlbumsRemoved(SongList);
  void SongsRemoved(SongList);

 private slots:
  void AddFavoritesReply(QNetworkReply *reply, const SpotifyFavoriteRequest::FavoriteType type, const SongList &songs);
  void RemoveFavoritesReply(QNetworkReply *reply, const SpotifyFavoriteRequest::FavoriteType type, const SongList &songs);

 public slots:
  void AddArtists(const SongList &songs);
  void AddAlbums(const SongList &songs);
  void AddSongs(const SongList &songs);
  void AddSongs(const SongMap &songs);

  void RemoveArtists(const SongList &songs);
  void RemoveAlbums(const SongList &songs);
  void RemoveSongs(const SongList &songs);
  void RemoveSongs(const SongMap &songs);

 private:
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
  static QString FavoriteText(const FavoriteType type);
  void AddFavorites(const FavoriteType type, const SongList &songs);
  void AddFavoritesRequest(const FavoriteType type, const QString &ids_list, const QByteArray &json_data, const SongList &songs);
  void RemoveFavorites(const FavoriteType type, const SongList &songs);
  void RemoveFavorites(const FavoriteType type, const QString &id, const SongList &songs);
  void RemoveFavoritesRequest(const FavoriteType type, const QString &ids_list, const QByteArray &json_data, const SongList &songs);

  SpotifyService *service_;
  NetworkAccessManager *network_;
  QList <QNetworkReply*> replies_;

};

#endif  // SPOTIFYFAVORITEREQUEST_H
