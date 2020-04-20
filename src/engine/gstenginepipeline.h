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

#ifndef GSTENGINEPIPELINE_H
#define GSTENGINEPIPELINE_H

#include "config.h"

#include <memory>
#include <glib.h>
#include <glib-object.h>
#include <glib/gtypes.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QThreadPool>
#include <QFuture>
#include <QTimeLine>
#include <QBasicTimer>
#include <QList>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>

class QTimerEvent;
class GstEngine;
class GstBufferConsumer;
class GstElementDeleter;

namespace Engine {
struct SimpleMetaBundle;
}  // namespace Engine
struct GstPlayBin;

class GstEnginePipeline : public QObject {
  Q_OBJECT

 public:
  explicit GstEnginePipeline(GstEngine *engine);
  ~GstEnginePipeline();

  // Globally unique across all pipelines.
  int id() const { return id_; }

  // Call these setters before Init
  void set_output_device(const QString &sink, const QVariant &device);
  void set_volume_enabled(const bool enabled);
  void set_stereo_balancer_enabled(const bool enabled);
  void set_equalizer_enabled(const bool enabled);
  void set_replaygain(const bool enabled, const int mode, const float preamp, const bool compression);
  void set_buffer_duration_nanosec(qint64 duration_nanosec);
  void set_buffer_min_fill(int percent);

  // Creates the pipeline, returns false on error
  bool InitFromUrl(const QByteArray &stream_url, const QUrl original_url, const qint64 end_nanosec);

  // GstBufferConsumers get fed audio data.  Thread-safe.
  void AddBufferConsumer(GstBufferConsumer *consumer);
  void RemoveBufferConsumer(GstBufferConsumer *consumer);
  void RemoveAllBufferConsumers();

  // Control the music playback
  QFuture<GstStateChangeReturn> SetState(const GstState state);
  Q_INVOKABLE bool Seek(const qint64 nanosec);
  void SetVolume(const int percent);
  void SetStereoBalance(const float value);
  void SetEqualizerParams(const int preamp, const QList<int> &band_gains);

  void StartFader(const qint64 duration_nanosec, const QTimeLine::Direction direction = QTimeLine::Forward, const QTimeLine::CurveShape shape = QTimeLine::LinearCurve, const bool use_fudge_timer = true);

  // If this is set then it will be loaded automatically when playback finishes for gapless playback
  void SetNextUrl(const QByteArray &stream_url, const QUrl &original_url, qint64 beginning_nanosec, qint64 end_nanosec);
  bool has_next_valid_url() const { return !next_stream_url_.isEmpty(); }

  void SetSourceDevice(QString device) { source_device_ = device; }

  // Get information about the music playback
  QByteArray stream_url() const { return stream_url_; }
  QUrl original_url() const { return original_url_; }
  bool is_valid() const { return valid_; }

  // Please note that this method (unlike GstEngine's.position()) is multiple-section media unaware.
  qint64 position() const;
  // Please note that this method (unlike GstEngine's.length()) is multiple-section media unaware.
  qint64 length() const;
  // Returns this pipeline's state. May return GST_STATE_NULL if the state check timed out. The timeout value is a reasonable default.
  GstState state() const;
  qint64 segment_start() const { return segment_start_; }

  // Don't allow the user to change the playback state (playing/paused) while the pipeline is buffering.
  bool is_buffering() const { return buffering_; }

  QByteArray redirect_url() const { return redirect_url_; }

  QString source_device() const { return source_device_; }

 public slots:
  void SetVolumeModifier(qreal mod);

 signals:
  void EndOfStreamReached(const int pipeline_id, const bool has_next_track);
  void MetadataFound(const int pipeline_id, const Engine::SimpleMetaBundle &bundle);
  // This indicates an error, delegated from GStreamer, in the pipeline.
  // The message, domain and error_code are related to GStreamer's GError.
  void Error(const int pipeline_id, const QString &message, const int domain, const int error_code);
  void FaderFinished();

  void BufferingStarted();
  void BufferingProgress(int percent);
  void BufferingFinished();

 protected:
  void timerEvent(QTimerEvent*);

 private:
  bool InitAudioBin();

  // Static callbacks.  The GstEnginePipeline instance is passed in the last argument.
  static GstPadProbeReturn EventHandoffCallback(GstPad*, GstPadProbeInfo*, gpointer);
  static void SourceSetupCallback(GstPlayBin*, GParamSpec* pspec, gpointer);
  static void NewPadCallback(GstElement*, GstPad*, gpointer);
  static GstPadProbeReturn PlaybinProbe(GstPad*, GstPadProbeInfo*, gpointer);
  static GstPadProbeReturn HandoffCallback(GstPad*, GstPadProbeInfo*, gpointer);
  static void AboutToFinishCallback(GstPlayBin*, gpointer);
  static GstBusSyncReply BusCallbackSync(GstBus*, GstMessage*, gpointer);
  static gboolean BusCallback(GstBus*, GstMessage*, gpointer);
  static void TaskEnterCallback(GstTask*, GThread*, gpointer);
  static void StreamDiscovered(GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, gpointer instance);
  static void StreamDiscoveryFinished(GstDiscoverer *discoverer, gpointer instance);
  static QString GSTdiscovererErrorMessage(GstDiscovererResult result);

  void TagMessageReceived(GstMessage*);
  void ErrorMessageReceived(GstMessage*);
  void ElementMessageReceived(GstMessage*);
  void StateChangedMessageReceived(GstMessage*);
  void BufferingMessageReceived(GstMessage*);
  void StreamStatusMessageReceived(GstMessage*);
  void StreamStartMessageReceived();

  QString ParseStrTag(GstTagList *list, const char *tag) const;
  guint ParseUIntTag(GstTagList *list, const char *tag) const;

  void UpdateVolume();
  void UpdateStereoBalance();
  void UpdateEqualizer();

 private slots:
  void FaderTimelineFinished();

 private:
  static const int kGstStateTimeoutNanosecs;
  static const int kFaderFudgeMsec;
  static const int kDiscoveryTimeoutS;
  static const int kEqBandCount;
  static const int kEqBandFrequencies[];

  static GstElementDeleter *sElementDeleter;

  GstEngine *engine_;

  // Using == to compare two pipelines is a bad idea, because new ones often get created in the same address as old ones.  This ID will be unique for each pipeline.
  // Threading warning: access to the static ID field isn't protected by a mutex because all pipeline creation is currently done in the main thread.
  static int sId;
  int id_;

  // General settings for the pipeline
  bool valid_;
  QString output_;
  QVariant device_;
  bool volume_enabled_;
  bool stereo_balancer_enabled_;
  bool eq_enabled_;
  bool rg_enabled_;

  // Stereo balance:
  // From -1.0 - 1.0
  // -1.0 is left, 1.0 is right.
  float stereo_balance_;

  // Equalizer
  int eq_preamp_;
  QList<int> eq_band_gains_;

  // ReplayGain
  int rg_mode_;
  float rg_preamp_;
  bool rg_compression_;

  // Buffering
  quint64 buffer_duration_nanosec_;
  int buffer_min_fill_;
  bool buffering_;

  // These get called when there is a new audio buffer available
  QList<GstBufferConsumer*> buffer_consumers_;
  QMutex buffer_consumers_mutex_;
  qint64 segment_start_;
  bool segment_start_received_;

  // The URL that is currently playing, and the URL that is to be preloaded when the current track is close to finishing.
  QByteArray stream_url_;
  QUrl original_url_;
  QByteArray next_stream_url_;
  QUrl next_original_url_;

  // If this is > 0 then the pipeline will be forced to stop when playback goes past this position.
  qint64 end_offset_nanosec_;

  // We store the beginning and end for the preloading song too, so we can just carry on without reloading the file if the sections carry on from each other.
  qint64 next_beginning_offset_nanosec_;
  qint64 next_end_offset_nanosec_;

  // Set temporarily when moving to the next contiguous section in a multi-part file.
  bool ignore_next_seek_;

  // Set temporarily when switching out the decode bin, so metadata doesn't get sent while the Player still thinks it's playing the last song
  bool ignore_tags_;

  // When the gstreamer source requests a redirect we store the URL here and callers can pick it up after the state change to PLAYING fails.
  QByteArray redirect_url_;

  // When we need to specify the device to use as source (for CD device)
  QString source_device_;

  // Seeking while the pipeline is in the READY state doesn't work, so we have to wait until it goes to PAUSED or PLAYING.
  // Also we have to wait for the playbin to be connected.
  bool pipeline_is_initialised_;
  bool pipeline_is_connected_;
  qint64 pending_seek_nanosec_;

  // We can only use gst_element_query_position() when the pipeline is in
  // PAUSED nor PLAYING state. Whenever we get a new position (e.g. after a correct call to gst_element_query_position() or after a seek), we store
  // it here so that we can use it when using gst_element_query_position() is not possible.
  mutable gint64 last_known_position_ns_;

  // Complete the transition to the next song when it starts playing
  bool next_uri_set_;

  int volume_percent_;
  qreal volume_modifier_;

  std::unique_ptr<QTimeLine> fader_;
  QBasicTimer fader_fudge_timer_;
  bool use_fudge_timer_;

  GstElement *pipeline_;
  GstElement *audiobin_;
  GstElement *audioqueue_;
  GstElement *volume_;
  GstElement *audiopanorama_;
  GstElement *equalizer_;
  GstElement *equalizer_preamp_;
  GstDiscoverer *discoverer_;

  int pad_added_cb_id_;
  int notify_source_cb_id_;
  int about_to_finish_cb_id_;
  int bus_cb_id_;
  int discovery_finished_cb_id_;
  int discovery_discovered_cb_id_;

  QThreadPool set_state_threadpool_;

  GstSegment last_playbin_segment_;

};

#endif  // GSTENGINEPIPELINE_H
