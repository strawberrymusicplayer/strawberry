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

#ifndef SPOTIFYSTREAMURLREQUEST_H
#define SPOTIFYSTREAMURLREQUEST_H

#include "config.h"

#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include "core/song.h"

class SpotifyService;

class SpotifyStreamURLRequest : public QObject {
  Q_OBJECT

 public:
  explicit SpotifyStreamURLRequest(SpotifyService *service, const QUrl &media_url, const uint id, QObject *parent = nullptr);

  void Process();

  QUrl media_url() const { return media_url_; }
  QString song_id() const { return song_id_; }

 Q_SIGNALS:
  void StreamURLFailure(const uint id, const QUrl &media_url, const QString &error);
  void StreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate = -1, const int bit_depth = -1, const qint64 duration = -1);

 private:
  SpotifyService *service_;
  QUrl media_url_;
  uint id_;
  QString song_id_;
};

using SpotifyStreamURLRequestPtr = QSharedPointer<SpotifyStreamURLRequest>;

#endif  // SPOTIFYSTREAMURLREQUEST_H
