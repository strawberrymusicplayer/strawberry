/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef CDDASONGLOADER_H
#define CDDASONGLOADER_H

#include "config.h"

#include <gst/gstelement.h>
#include <gst/audio/gstaudiocdsrc.h>

#include <QObject>
#include <QMutex>
#include <QString>
#include <QUrl>
#include <QFuture>

#include "includes/shared_ptr.h"
#include "core/song.h"
#ifdef HAVE_MUSICBRAINZ
#  include "musicbrainz/musicbrainzclient.h"
#endif

class NetworkAccessManager;

class CDDASongLoader : public QObject {
  Q_OBJECT

 public:
  explicit CDDASongLoader(const QUrl &url, QObject *parent = nullptr);
  ~CDDASongLoader() override;

  void LoadSongs();

  bool IsActive() const { return loading_future_.isRunning(); }

 private:
  void LoadSongsFromCDDA();
  void Error(const QString &error);
  QUrl GetUrlFromTrack(const int track_number) const;

 Q_SIGNALS:
  void SongsLoaded(const SongList &songs);
  void SongsUpdated(const SongList &songs);
  void LoadError(const QString &error);
  void LoadingFinished();
  void LoadTagsFromMusicBrainz(const QString &musicbrainz_discid);

 private Q_SLOTS:
#ifdef HAVE_MUSICBRAINZ
  void LoadTagsFromMusicBrainzSlot(const QString &musicbrainz_discid) const;
  void LoadTagsFromMusicBrainzFinished(const QString &artist, const QString &album, const MusicBrainzClient::ResultList &results, const QString &error);
#endif

 private:
  const QUrl url_;
  SharedPtr<NetworkAccessManager> network_;
  QMutex mutex_load_;
  QFuture<void> loading_future_;
};

#endif  // CDDASONGLOADER_H
