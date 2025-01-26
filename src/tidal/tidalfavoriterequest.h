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

#ifndef TIDALFAVORITEREQUEST_H
#define TIDALFAVORITEREQUEST_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"

#include "tidalbaserequest.h"

class QNetworkReply;
class TidalService;
class NetworkAccessManager;

class TidalFavoriteRequest : public TidalBaseRequest {
  Q_OBJECT

 public:
  explicit TidalFavoriteRequest(TidalService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

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
  void AddFavoritesReply(QNetworkReply *reply, const TidalFavoriteRequest::FavoriteType type, const SongList &songs);
  void RemoveFavoritesReply(QNetworkReply *reply, const TidalFavoriteRequest::FavoriteType type, const SongList &songs);

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
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
  static QString FavoriteText(const FavoriteType type);
  static QString FavoriteMethod(const FavoriteType type);
  void AddFavorites(const FavoriteType type, const SongList &songs);
  void AddFavoritesRequest(const FavoriteType type, const QStringList &id_list, const SongList &songs);
  void RemoveFavorites(const FavoriteType type, const SongList &songs);
  void RemoveFavorites(const FavoriteType type, const QString &id, const SongList &songs);
  void RemoveFavoritesRequest(const FavoriteType type, const QString &id, const SongList &songs);

  TidalService *service_;
  const SharedPtr<NetworkAccessManager> network_;
};

#endif  // TIDALFAVORITEREQUEST_H
