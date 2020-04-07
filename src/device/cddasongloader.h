/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QObject>
#include <QMutex>
#include <QString>
#include <QUrl>

// These must come after Qt includes
#include <cdio/types.h>
#include <cdio/cdio.h>
#include <gst/gstelement.h>
#include <gst/audio/gstaudiocdsrc.h>

#include "core/song.h"
#ifdef HAVE_CHROMAPRINT
#  include "musicbrainz/musicbrainzclient.h"
#endif

// This class provides a (hopefully) nice, high level interface to get CD information and load tracks
class CddaSongLoader : public QObject {
  Q_OBJECT

 public:
  explicit CddaSongLoader(const QUrl &url = QUrl(), QObject *parent = nullptr);
  ~CddaSongLoader();

  // Load songs. Signals declared below will be emitted anytime new information will be available.
  void LoadSongs();
  bool HasChanged();

 private:
  void Error(const QString &error);
  QUrl GetUrlFromTrack(const int track_number) const;

 signals:
  void SongsLoadError(const QString &error);
  void SongsLoaded(const SongList &songs);
  void SongsDurationLoaded(const SongList &songs, const QString &error = QString());
  void SongsMetadataLoaded(const SongList &songs);

 private slots:
#ifdef HAVE_CHROMAPRINT
  void AudioCDTagsLoaded(const QString &artist, const QString &album, const MusicBrainzClient::ResultList &results);
#endif

 private:
  QUrl url_;
  GstElement *cdda_;
  CdIo_t *cdio_;
  QMutex mutex_load_;
};

#endif // CDDASONGLOADER_H
