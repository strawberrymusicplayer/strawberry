/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QMap>
#include <QMultiMap>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "tidalservice.h"
#include "tidalbaserequest.h"
#include "tidalfavoriterequest.h"

using namespace Qt::StringLiterals;

TidalFavoriteRequest::TidalFavoriteRequest(TidalService *service, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : TidalBaseRequest(service, network, parent),
      service_(service),
      network_(network),
      need_login_(false) {}

TidalFavoriteRequest::~TidalFavoriteRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

QString TidalFavoriteRequest::FavoriteText(const FavoriteType type) {

  switch (type) {
    case FavoriteType::Artists:
      return QStringLiteral("artists");
    case FavoriteType::Albums:
      return QStringLiteral("albums");
    case FavoriteType::Songs:
      return QStringLiteral("tracks");
  }

  return QString();

}

QString TidalFavoriteRequest::FavoriteMethod(const FavoriteType type) {

  switch (type) {
    case FavoriteType::Artists:
      return QStringLiteral("artistIds");
    case FavoriteType::Albums:
      return QStringLiteral("albumIds");
    case FavoriteType::Songs:
      return QStringLiteral("trackIds");
  }

  return QString();

}

void TidalFavoriteRequest::AddArtists(const SongList &songs) {
  AddFavorites(FavoriteType::Artists, songs);
}

void TidalFavoriteRequest::AddAlbums(const SongList &songs) {
  AddFavorites(FavoriteType::Albums, songs);
}

void TidalFavoriteRequest::AddSongs(const SongList &songs) {
  AddFavorites(FavoriteType::Songs, songs);
}

void TidalFavoriteRequest::AddSongs(const SongMap &songs) {
  AddFavoritesRequest(FavoriteType::Songs, songs.keys(), songs.values());
}

void TidalFavoriteRequest::AddFavorites(const FavoriteType type, const SongList &songs) {

  QStringList id_list;
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
    if (!id.isEmpty() && !id_list.contains(id)) {
      id_list << id;
    }
  }

  if (id_list.isEmpty()) return;

  AddFavoritesRequest(type, id_list, songs);

}

void TidalFavoriteRequest::AddFavoritesRequest(const FavoriteType type, const QStringList &id_list, const SongList &songs) {

  const ParamList params = ParamList() << Param(QStringLiteral("countryCode"), country_code())
                                       << Param(FavoriteMethod(type), id_list.join(u','));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(TidalService::kApiUrl) + QLatin1Char('/') + "users/"_L1 + QString::number(service_->user_id()) + "/favorites/"_L1 + FavoriteText(type));
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  if (oauth() && !access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + access_token().toUtf8());
  else if (!session_id().isEmpty()) req.setRawHeader("X-Tidal-SessionId", session_id().toUtf8());
  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { AddFavoritesReply(reply, type, songs); });
  replies_ << reply;

  qLog(Debug) << "Tidal: Sending request" << url << query;

}

void TidalFavoriteRequest::AddFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();
  }
  else {
    return;
  }

  GetReplyData(reply, false);

  if (reply->error() != QNetworkReply::NoError) {
    return;
  }

  qLog(Debug) << "Tidal:" << songs.count() << "songs added to" << FavoriteText(type) << "favorites.";

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

void TidalFavoriteRequest::RemoveArtists(const SongList &songs) {
  RemoveFavorites(FavoriteType::Artists, songs);
}

void TidalFavoriteRequest::RemoveAlbums(const SongList &songs) {
  RemoveFavorites(FavoriteType::Albums, songs);
}

void TidalFavoriteRequest::RemoveSongs(const SongList &songs) {
  RemoveFavorites(FavoriteType::Songs, songs);
}

void TidalFavoriteRequest::RemoveSongs(const SongMap &songs) {

  const SongList songs_list = songs.values();
  for (const Song &song : songs_list) {
    RemoveFavoritesRequest(FavoriteType::Songs, song.song_id(), SongList() << song);
  }

}

void TidalFavoriteRequest::RemoveFavorites(const FavoriteType type, const SongList &songs) {

  QMultiMap<QString, Song> songs_map;
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
    if (!id.isEmpty()) {
      songs_map.insert(id, song);
    }
  }

  const QStringList ids = songs_map.uniqueKeys();
  for (const QString &id : ids) {
    RemoveFavoritesRequest(type, id, songs_map.values(id));
  }

}

void TidalFavoriteRequest::RemoveFavoritesRequest(const FavoriteType type, const QString &id, const SongList &songs) {

  const ParamList params = ParamList() << Param(QStringLiteral("countryCode"), country_code());

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(TidalService::kApiUrl) + "/users/"_L1 + QString::number(service_->user_id()) + "/favorites/"_L1 + FavoriteText(type) + "/"_L1 + id);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  if (oauth() && !access_token().isEmpty()) req.setRawHeader("authorization", "Bearer " + access_token().toUtf8());
  else if (!session_id().isEmpty()) req.setRawHeader("X-Tidal-SessionId", session_id().toUtf8());
  QNetworkReply *reply = network_->deleteResource(req);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type, songs]() { RemoveFavoritesReply(reply, type, songs); });
  replies_ << reply;

  qLog(Debug) << "Tidal: Sending request" << url << "with" << songs.count() << "songs";

}

void TidalFavoriteRequest::RemoveFavoritesReply(QNetworkReply *reply, const FavoriteType type, const SongList &songs) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();
  }
  else {
    return;
  }

  GetReplyData(reply, false);
  if (reply->error() != QNetworkReply::NoError) {
    return;
  }

  qLog(Debug) << "Tidal:" << songs.count() << "songs removed from" << FavoriteText(type) << "favorites.";

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

void TidalFavoriteRequest::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
