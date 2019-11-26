/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SONGLOADER_H
#define SONGLOADER_H

#include "config.h"

#include <memory>
#include <functional>
#include <glib.h>
#include <stdbool.h>

#ifdef HAVE_GSTREAMER
#  include <gst/gst.h>
#endif

#include <QtGlobal>
#include <QObject>
#include <QThreadPool>
#include <QByteArray>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QTimer>

#include "song.h"

class Player;
class CollectionBackendInterface;
class PlaylistParser;
class ParserBase;
class CueParser;

#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
class CddaSongLoader;
#endif

class SongLoader : public QObject {
  Q_OBJECT
 public:
  SongLoader(CollectionBackendInterface *collection, const Player *player, QObject *parent = nullptr);
  ~SongLoader();

  enum Result {
    Success,
    Error,
    BlockingLoadRequired,
  };

  static const int kDefaultTimeout;

  const QUrl &url() const { return url_; }
  const SongList &songs() const { return songs_; }

  int timeout() const { return timeout_; }
  void set_timeout(int msec) { timeout_ = msec; }

  // If Success is returned the songs are fully loaded. If BlockingLoadRequired is returned LoadFilenamesBlocking() needs to be called next.
  Result Load(const QUrl &url);
  // Loads the files with only filenames. When finished, songs() contains a complete list of all Song objects, but without metadata.
  // This method is blocking, do not call it from the UI thread.
  SongLoader::Result LoadFilenamesBlocking();
  // Completely load songs previously loaded with LoadFilenamesBlocking().
  // When finished, the Song objects in songs() contain metadata now. This method is blocking, do not call it from the UI thread.
  void LoadMetadataBlocking();
  Result LoadAudioCD();

  QStringList errors() { return errors_; }

 signals:
  void AudioCDTracksLoaded();
  void LoadAudioCDFinished(bool success);
  void LoadRemoteFinished();

 private slots:
  void Timeout();
  void StopTypefind();
#if defined(HAVE_AUDIOCD) && defined(HAVE_GSTREAMER)
  void AudioCDTracksLoadedSlot(const SongList &songs);
  void AudioCDTracksTagsLoaded(const SongList &songs);
#endif  // HAVE_AUDIOCD && HAVE_GSTREAMER

 private:
  enum State { WaitingForType, WaitingForMagic, WaitingForData, Finished };

  Result LoadLocal(const QString &filename);
  SongLoader::Result LoadLocalAsync(const QString &filename);
  void EffectiveSongLoad(Song *song);
  Result LoadLocalPartial(const QString &filename);
  void LoadLocalDirectory(const QString &filename);
  void LoadPlaylist(ParserBase *parser, const QString &filename);

  void AddAsRawStream();

#ifdef HAVE_GSTREAMER
  Result LoadRemote();

  // GStreamer callbacks
  static void TypeFound(GstElement *typefind, uint probability, GstCaps *caps, void *self);
  static GstPadProbeReturn DataReady(GstPad*, GstPadProbeInfo *buf, gpointer self);
  static GstBusSyncReply BusCallbackSync(GstBus*, GstMessage*, gpointer);
  static gboolean BusCallback(GstBus*, GstMessage*, gpointer);

  void StopTypefindAsync(bool success);
  void ErrorMessageReceived(GstMessage *msg);
  void EndOfStreamReached();
  void MagicReady();
  bool IsPipelinePlaying();
#endif

 private:
  static QSet<QString> sRawUriSchemes;

  QUrl url_;
  SongList songs_;

  QTimer *timeout_timer_;
  PlaylistParser *playlist_parser_;
  CueParser *cue_parser_;

  // For async loads
  std::function<Result()> preload_func_;
  int timeout_;
  State state_;
  bool success_;
  ParserBase *parser_;
  QString mime_type_;
  QByteArray buffer_;
  CollectionBackendInterface *collection_;
  const Player *player_;

#ifdef HAVE_GSTREAMER
  std::shared_ptr<GstElement> pipeline_;
#endif

  QThreadPool thread_pool_;

  QStringList errors_;

};

#endif  // SONGLOADER_H

