/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef ACOUSTIDCLIENT_H
#define ACOUSTIDCLIENT_H

#include "config.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"

class QNetworkReply;
class NetworkAccessManager;
class NetworkTimeouts;

class AcoustidClient : public QObject {
  Q_OBJECT

  // Gets a MBID from a Chromaprint fingerprint.
  // A fingerprint identifies one particular encoding of a song and is created by Fingerprinter.
  // An MBID identifies the actual song and can be passed to Musicbrainz to get metadata.
  // You can create one AcoustidClient and make multiple requests using it.
  // IDs are provided by the caller when a request is started and included in the Finished signal - they have no meaning to AcoustidClient.

 public:
  explicit AcoustidClient(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~AcoustidClient() override;

  // Network requests will be aborted after this interval.
  void SetTimeout(const int msec);

  // Starts a request and returns immediately.  Finished() will be emitted later with the same ID.
  void Start(const int id, const QString &fingerprint, int duration_msec);

  // Cancels the request with the given ID.  Finished() will never be emitted for that ID.  Does nothing if there is no request with the given ID.
  void Cancel(const int id);

  // Cancels all requests.  Finished() will never be emitted for any pending requests.
  void CancelAll();

 Q_SIGNALS:
  void Finished(const int id, const QStringList &mbid_list, const QString &error = QString());

 private Q_SLOTS:
  void RequestFinished(QNetworkReply *reply, const int id);

 private:
  SharedPtr<NetworkAccessManager> network_;
  NetworkTimeouts *timeouts_;
  QMap<int, QNetworkReply*> requests_;
};

#endif  // ACOUSTIDCLIENT_H
