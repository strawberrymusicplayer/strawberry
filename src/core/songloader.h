/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef SONGLOADER_H
#define SONGLOADER_H

#include "config.h"

#include <memory>
#include <functional>
#include <glib.h>
#include <gst/gst.h>

#include <QtGlobal>
#include <QObject>
#include <QThreadPool>
#include <QByteArray>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"

class QTimer;
class UrlHandlers;
class TagReaderClient;
class CollectionBackendInterface;
class PlaylistParser;
class ParserBase;
class CueParser;

#ifdef HAVE_AUDIOCD
class CDDASongLoader;
#endif

class SongLoader : public QObject {
  Q_OBJECT

 public:
  explicit SongLoader(const SharedPtr<UrlHandlers> url_handlers,
                      const SharedPtr<CollectionBackendInterface> collection_backend,
                      const SharedPtr<TagReaderClient> tagreader_client,
                      QObject *parent = nullptr);

  ~SongLoader() override;

  enum class Result {
    Success,
    Error,
    BlockingLoadRequired
  };

  const QUrl &url() const { return url_; }
  const SongList &songs() const { return songs_; }
  const QString &playlist_name() const { return playlist_name_; }

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

 Q_SIGNALS:
  void AudioCDTracksLoaded();
  void AudioCDTracksUpdated();
  void AudioCDLoadingFinished(const bool success);
  void LoadRemoteFinished();

 private Q_SLOTS:
  void ScheduleTimeout();
  void Timeout();
  void StopTypefind();

#ifdef HAVE_AUDIOCD
  void AudioCDTracksLoadErrorSlot(const QString &error);
  void AudioCDTracksLoadedSlot(const SongList &songs);
  void AudioCDTracksUpdatedSlot(const SongList &songs);
  void AudioCDLoadingFinishedSlot();
#endif  // HAVE_AUDIOCD

 private:
  enum class State {
    WaitingForType,
    WaitingForMagic,
    WaitingForData,
    Finished
  };

  Result LoadLocal(const QString &filename);
  SongLoader::Result LoadLocalAsync(const QString &filename);
  void EffectiveSongLoad(Song *song);
  Result LoadLocalPartial(const QString &filename);
  void LoadLocalDirectory(const QString &filename);
  void LoadPlaylist(ParserBase *parser, const QString &filename);

  void AddAsRawStream();

  Result LoadRemote();

  // GStreamer callbacks
  static void TypeFound(GstElement *typefind, const uint probability, GstCaps *caps, void *self);
  static GstPadProbeReturn DataReady(GstPad *pad, GstPadProbeInfo *info, gpointer self);
  static GstBusSyncReply BusCallbackSync(GstBus *bus, GstMessage *msg, gpointer self);
  static gboolean BusWatchCallback(GstBus *bus, GstMessage *msg, gpointer self);

  void ErrorMessageReceived(GstMessage *msg);
  void EndOfStreamReached();
  void MagicReady();
  bool IsPipelinePlaying();
  void StopTypefindAsync(const bool success);
  void CleanupPipeline();

  void ScheduleTimeoutAsync();

 private:
  static QSet<QString> sRawUriSchemes;

  QUrl url_;
  SongList songs_;
  QString playlist_name_;

  const SharedPtr<UrlHandlers> url_handlers_;
  const SharedPtr<CollectionBackendInterface> collection_backend_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  QTimer *timeout_timer_;
  PlaylistParser *playlist_parser_;
  CueParser *cue_parser_;

  // For async loads
  std::function<Result()> preload_func_;
  QString mime_type_;
  QByteArray buffer_;
  ParserBase *parser_;
  State state_;
  int timeout_;

  SharedPtr<GstElement> pipeline_;
  GstElement *fakesink_;
  gulong buffer_probe_cb_id_;

  QThreadPool thread_pool_;
  QStringList errors_;

  bool success_;

};

#endif  // SONGLOADER_H
