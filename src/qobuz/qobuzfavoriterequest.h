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

#ifndef QOBUZFAVORITEREQUEST_H
#define QOBUZFAVORITEREQUEST_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>

#include "qobuzbaserequest.h"
#include "core/song.h"

class QNetworkReply;
class QobuzService;
class NetworkAccessManager;

class QobuzFavoriteRequest : public QobuzBaseRequest {
  Q_OBJECT

 public:
  QobuzFavoriteRequest(QobuzService *service, NetworkAccessManager *network, QObject *parent);
  ~QobuzFavoriteRequest();

  enum FavoriteType {
    FavoriteType_Artists,
    FavoriteType_Albums,
    FavoriteType_Songs
  };

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
  void Error(const QString &error, const QVariant &debug = QVariant());
  QString FavoriteText(const FavoriteType type);
  void AddFavorites(const FavoriteType type, const SongList &songs);
  void RemoveFavorites(const FavoriteType type, const SongList &songs);

  QobuzService *service_;
  NetworkAccessManager *network_;
  QList<QNetworkReply*> replies_;

};

#endif  // QOBUZFAVORITEREQUEST_H
