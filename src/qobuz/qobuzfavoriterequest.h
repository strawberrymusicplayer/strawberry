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
  explicit QobuzFavoriteRequest(QobuzService *service, NetworkAccessManager *network, QObject *parent = nullptr);
  ~QobuzFavoriteRequest();

 private:
  enum class FavoriteType {
    Artists,
    Albums,
    Songs
  };

 signals:
  void ArtistsAdded(SongList);
  void AlbumsAdded(SongList);
  void SongsAdded(SongList);
  void ArtistsRemoved(SongList);
  void AlbumsRemoved(SongList);
  void SongsRemoved(SongList);

 private slots:
  void AddFavoritesReply(QNetworkReply *reply, const QobuzFavoriteRequest::FavoriteType type, const SongList &songs);
  void RemoveFavoritesReply(QNetworkReply *reply, const QobuzFavoriteRequest::FavoriteType type, const SongList &songs);

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
  void Error(const QString &error, const QVariant &debug = QVariant());
  static QString FavoriteText(const FavoriteType type);
  static QString FavoriteMethod(const FavoriteType type);
  void AddFavorites(const FavoriteType type, const SongList &songs);
  void AddFavoritesRequest(const FavoriteType type, const QStringList &ids_list, const SongList &songs);
  void RemoveFavorites(const FavoriteType type, const SongList &songs);
  void RemoveFavoritesRequest(const FavoriteType type, const QStringList &ids_list, const SongList &songs);

  QobuzService *service_;
  NetworkAccessManager *network_;
  QList<QNetworkReply*> replies_;

};

#endif  // QOBUZFAVORITEREQUEST_H
