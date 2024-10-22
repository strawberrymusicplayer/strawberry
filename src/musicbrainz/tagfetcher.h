/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef TAGFETCHER_H
#define TAGFETCHER_H

#include "config.h"

#include <QObject>
#include <QFutureWatcher>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "musicbrainzclient.h"

class NetworkAccessManager;
class AcoustidClient;

class TagFetcher : public QObject {
  Q_OBJECT

  // High level interface to Fingerprinter, AcoustidClient and MusicBrainzClient.

 public:
  explicit TagFetcher(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  void StartFetch(const SongList &songs);

 public Q_SLOTS:
  void Cancel();

 Q_SIGNALS:
  void Progress(const Song &original_song, const QString &stage);
  void ResultAvailable(const Song &original_song, const SongList &songs_guessed, const QString &error = QString());

 private Q_SLOTS:
  void FingerprintFound(const int index);
  void PuidsFound(const int index, const QStringList &puid_list, const QString &error = QString());
  void TagsFetched(const int index, const MusicBrainzClient::ResultList &results, const QString &error = QString());

 private:
  static QString GetFingerprint(const Song &song);

  QFutureWatcher<QString> *fingerprint_watcher_;
  AcoustidClient *acoustid_client_;
  MusicBrainzClient *musicbrainz_client_;

  SongList songs_;
};

#endif  // TAGFETCHER_H
