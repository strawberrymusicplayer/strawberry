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

#ifndef TIDALFAVORITEREQUEST_H
#define TIDALFAVORITEREQUEST_H

#include "config.h"

#include <QList>

#include "tidalbaserequest.h"
#include "core/song.h"

class QNetworkReply;
class TidalService;
class NetworkAccessManager;

class TidalFavoriteRequest : public TidalBaseRequest {
  Q_OBJECT

 public:
  TidalFavoriteRequest(TidalService *service, NetworkAccessManager *network, QObject *parent);
  ~TidalFavoriteRequest();

  enum FavoriteType {
    FavoriteType_Artists,
    FavoriteType_Albums,
    FavoriteType_Songs
  };

  bool need_login() { return need_login_; }

  void NeedLogin() { need_login_ = true; }

 signals:
  void ArtistsAdded(const SongList &songs);
  void AlbumsAdded(const SongList &songs);
  void SongsAdded(const SongList &songs);
  void ArtistsRemoved(const SongList &songs);
  void AlbumsRemoved(const SongList &songs);
  void SongsRemoved(const SongList &songs);

 private slots:
  void AddArtists(const SongList &songs);
  void AddAlbums(const SongList &songs);
  void AddSongs(const SongList &songs);

  void RemoveArtists(const SongList &songs);
  void RemoveAlbums(const SongList &songs);
  void RemoveSongs(const SongList &songs);

  void AddFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs);
  void RemoveFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs);

 private:
  QString FavoriteText(const FavoriteType type);
  void AddFavorites(const FavoriteType type, const SongList &songs);
  void RemoveFavorites(const FavoriteType type, const SongList songs);
  void RemoveFavorites(const FavoriteType type, const int id, const SongList &songs);

  TidalService *service_;
  NetworkAccessManager *network_;
  QList <QNetworkReply*> replies_;
  bool need_login_;

};

#endif  // TIDALFAVORITEREQUEST_H
