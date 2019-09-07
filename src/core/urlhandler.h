/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef URLHANDLER_H
#define URLHANDLER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QUrl>

#include "song.h"

class UrlHandler : public QObject {
  Q_OBJECT

 public:
  explicit UrlHandler(QObject *parent = nullptr);

  // The URL scheme that this handler handles.
  virtual QString scheme() const = 0;

  // Returned by StartLoading() and LoadNext(), indicates what the player should do when it wants to load a URL.
  struct LoadResult {
    enum Type {
      // There wasn't a track available, and the player should move on to the next playlist item.
      NoMoreTracks,

      // There might be another track available but the handler needs to do some work (eg. fetching a remote playlist) to find out.
      // AsyncLoadComplete will be emitted later with the same original_url.
      WillLoadAsynchronously,

      // There was a track available.  Its url is in stream_url.
      TrackAvailable,

      // There was a error
      Error,
    };

    LoadResult(const QUrl &original_url = QUrl(), const Type type = NoMoreTracks, const QUrl &stream_url = QUrl(), const Song::FileType filetype = Song::FileType_Stream, const int samplerate = -1, const int bitdepth = -1, const qint64 length_nanosec_ = -1, const QString error = QString());

    // The url that the playlist item has in Url().
    // Might be something unplayable like lastfm://...
    QUrl original_url_;

    Type type_;

    // The actual url to something that gstreamer can play.
    QUrl stream_url_;

    // The type of the stream
    Song::FileType filetype_;

    // Track sample rate
    int samplerate_;

    // Track bit depth
    int bit_depth_;

    // Track length
    qint64 length_nanosec_;

    // Error message, if any
    QString error_;
  };

  // Called by the Player when a song starts loading - gives the handler a chance to do something clever to get a playable track.
  virtual LoadResult StartLoading(const QUrl &url) { return LoadResult(url); }

 signals:
  void AsyncLoadComplete(const UrlHandler::LoadResult &result);

};

#endif  // URLHANDLER_H
