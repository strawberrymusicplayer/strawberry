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

#ifndef QOBUZFAVORITEREQUEST_H
#define QOBUZFAVORITEREQUEST_H

#include "config.h"

#include <QVariant>
#include <QString>

#include "qobuzbaserequest.h"
#include "includes/shared_ptr.h"
#include "core/song.h"

class QNetworkReply;
class QobuzService;
class NetworkAccessManager;
class QobuzService;

class QobuzFavoriteRequest : public QobuzBaseRequest {
  Q_OBJECT

 public:
  explicit QobuzFavoriteRequest(QobuzService *service, SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

 private:
  enum class FavoriteType {
    Artists,
    Albums,
    Songs
  };

 Q_SIGNALS:
  void ArtistsAdded(const SongList &songs);
  void AlbumsAdded(const SongList &songs);
  void SongsAdded(const SongList &songs);
  void ArtistsRemoved(const SongList &songs);
  void AlbumsRemoved(const SongList &songs);
  void SongsRemoved(const SongList &songs);

 private Q_SLOTS:
  void AddFavoritesReply(QNetworkReply *reply, const QobuzFavoriteRequest::FavoriteType type, const SongList &songs);
  void RemoveFavoritesReply(QNetworkReply *reply, const QobuzFavoriteRequest::FavoriteType type, const SongList &songs);

 public Q_SLOTS:
  void AddArtists(const SongList &songs);
  void AddAlbums(const SongList &songs);
  void AddSongs(const SongList &songs);
  void AddSongs(const SongMap &songs);
  void RemoveArtists(const SongList &songs);
  void RemoveAlbums(const SongList &songs);
  void RemoveSongs(const SongList &songs);
  void RemoveSongs(const SongMap &songs);

 private:
  void Error(const QString &error, const QVariant &debug = QVariant());
  static QString FavoriteText(const FavoriteType type);
  static QString FavoriteMethod(const FavoriteType type);
  void AddFavorites(const FavoriteType type, const SongList &songs);
  void AddFavoritesRequest(const FavoriteType type, const QStringList &ids_list, const SongList &songs);
  void RemoveFavorites(const FavoriteType type, const SongList &songs);
  void RemoveFavoritesRequest(const FavoriteType type, const QStringList &ids_list, const SongList &songs);
};

#endif  // QOBUZFAVORITEREQUEST_H
