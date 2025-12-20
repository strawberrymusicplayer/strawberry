/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SPOTIFYMETADATAREQUEST_H
#define SPOTIFYMETADATAREQUEST_H

#include "config.h"

#include <QObject>
#include <QString>

#include "includes/shared_ptr.h"
#include "spotifybaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class SpotifyService;

class SpotifyMetadataRequest : public SpotifyBaseRequest {
  Q_OBJECT

 public:
  explicit SpotifyMetadataRequest(SpotifyService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  void FetchTrackMetadata(const QString &track_id);

 Q_SIGNALS:
  void MetadataReceived(QString track_id, QString genre);
  void MetadataFailure(QString track_id, QString error);

 private Q_SLOTS:
  void TrackMetadataReceived(QNetworkReply *reply, const QString &track_id);
  void ArtistMetadataReceived(QNetworkReply *reply, const QString &track_id);

 private:
  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;
};

#endif  // SPOTIFYMETADATAREQUEST_H
