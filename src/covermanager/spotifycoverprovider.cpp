/*
 * Strawberry Music Player
 * Copyright 2020-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QMessageBox>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "spotify/spotifyservice.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "spotifycoverprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kApiUrl[] = "https://api.spotify.com/v1";
constexpr int kLimit = 10;
}  // namespace

SpotifyCoverProvider::SpotifyCoverProvider(const SpotifyServicePtr service, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Spotify"_s, true, true, 2.5, true, true, network, parent),
      service_(service) {}

SpotifyCoverProvider::~SpotifyCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool SpotifyCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (!IsAuthenticated()) return false;

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString type;
  QString extract;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    type = "track"_L1;
    extract = "tracks"_L1;
    if (!query.isEmpty()) query.append(u' ');
    query.append(title);
  }
  else {
    type = "album"_L1;
    extract = "albums"_L1;
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(u' ');
      query.append(album);
    }
  }

  const ParamList params = ParamList() << Param(u"q"_s, query)
                                       << Param(u"type"_s, type)
                                       << Param(u"limit"_s, QString::number(kLimit));

  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(kApiUrl) + u"/search"_s), params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, extract]() { HandleSearchReply(reply, id, extract); });

  return true;

}

void SpotifyCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

void SpotifyCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &extract) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const QByteArray data = GetReplyData(reply).data;
  if (data.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QJsonObject json_object = ExtractJsonObj(data);
  if (json_object.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  if (!json_object.contains(extract) || !json_object[extract].isObject()) {
    Error(QStringLiteral("Json object is missing %1 object.").arg(extract), json_object);
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }
  json_object = json_object[extract].toObject();

  if (!json_object.contains("items"_L1) || !json_object["items"_L1].isArray()) {
    Error(QStringLiteral("%1 object is missing items array.").arg(extract), json_object);
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  const QJsonArray array_items = json_object["items"_L1].toArray();
  if (array_items.isEmpty()) {
    Q_EMIT SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  CoverProviderSearchResults results;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      continue;
    }
    QJsonObject object_item = value_item.toObject();

    QJsonObject obj_album = object_item;
    if (object_item.contains("album"_L1) && object_item["album"_L1].isObject()) {
      obj_album = object_item["album"_L1].toObject();
    }

    if (!obj_album.contains("artists"_L1) || !obj_album.contains("name"_L1) || !obj_album.contains("images"_L1) || !obj_album["artists"_L1].isArray() || !obj_album["images"_L1].isArray()) {
      continue;
    }
    const QJsonArray array_artists = obj_album["artists"_L1].toArray();
    const QJsonArray array_images = obj_album["images"_L1].toArray();
    const QString album = obj_album["name"_L1].toString();

    QStringList artists;
    for (const QJsonValue &value_artist : array_artists) {
      if (!value_artist.isObject()) continue;
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains("name"_L1)) continue;
      artists << obj_artist["name"_L1].toString();
    }

    for (const QJsonValue &value_image : array_images) {
      if (!value_image.isObject()) continue;
      const QJsonObject object_image = value_image.toObject();
      if (!object_image.contains("url"_L1) || !object_image.contains("width"_L1) || !object_image.contains("height"_L1)) continue;
      const int width = object_image["width"_L1].toInt();
      const int height = object_image["height"_L1].toInt();
      if (width < 300 || height < 300) continue;
      const QUrl url(object_image["url"_L1].toString());
      CoverProviderSearchResult result;
      result.album = album;
      result.image_url = url;
      result.image_size = QSize(width, height);
      if (!artists.isEmpty()) result.artist = artists.first();
      results << result;
    }

  }
  Q_EMIT SearchFinished(id, results);

}

void SpotifyCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Spotify:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
