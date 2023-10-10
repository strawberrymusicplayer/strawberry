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

#ifndef GSTENGINEPIPELINE_H
#define GSTENGINEPIPELINE_H

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gtypes.h>
#include <gst/gst.h>

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QThreadPool>
#include <QFuture>
#include <QTimeLine>
#include <QEasingCurve>
#include <QBasicTimer>
#include <QList>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "core/shared_ptr.h"
#include "enginemetadata.h"

class QTimerEvent;
class GstBufferConsumer;
struct GstPlayBin;

class GstEnginePipeline : public QObject {
  Q_OBJECT

 public:
  explicit GstEnginePipeline(QObject *parent = nullptr);
  ~GstEnginePipeline() override;

  // Globally unique across all pipelines.
  int id() const { return id_; }

  // Call these setters before Init
  void set_output_device(const QString &output, const QVariant &device);
  void set_volume_enabled(const bool enabled);
  void set_stereo_balancer_enabled(const bool enabled);
  void set_equalizer_enabled(const bool enabled);
  void set_replaygain(const bool enabled, const int mode, const double preamp, const double fallbackgain, const bool compression);
  void set_ebur128_loudness_normalization(const bool enabled);
  void set_buffer_duration_nanosec(const quint64 duration_nanosec);
  void set_buffer_low_watermark(const double value);
  void set_buffer_high_watermark(const double value);
  void set_proxy_settings(const QString &address, const bool authentication, const QString &user, const QString &pass);
  void set_channels(const bool enabled, const int channels);
  void set_bs2b_enabled(const bool enabled);
  void set_strict_ssl_enabled(const bool enabled);
  void set_fading_enabled(const bool enabled);

  // Creates the pipeline, returns false on error
  bool InitFromUrl(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 end_nanosec, const double ebur128_loudness_normalizing_gain_db, QString &error);

  // GstBufferConsumers get fed audio data.  Thread-safe.
  void AddBufferConsumer(GstBufferConsumer *consumer);
  void RemoveBufferConsumer(GstBufferConsumer *consumer);
  void RemoveAllBufferConsumers();

  // Control the music playback
  Q_INVOKABLE QFuture<GstStateChangeReturn> SetState(const GstState state);
  void SetStateDelayed(const GstState state);
  Q_INVOKABLE bool Seek(const qint64 nanosec);
  void SeekQueued(const qint64 nanosec);
  void SeekDelayed(const qint64 nanosec);
  void SetEBUR128LoudnessNormalizingGain_dB(const double ebur128_loudness_normalizing_gain_db);
  void SetVolume(const uint volume_percent);
  void SetStereoBalance(const float value);
  void SetEqualizerParams(const int preamp, const QList<int> &band_gains);

  void StartFader(const qint64 duration_nanosec, const QTimeLine::Direction direction = QTimeLine::Forward, const QEasingCurve::Type shape = QEasingCurve::Linear, const bool use_fudge_timer = true);

  // If this is set then it will be loaded automatically when playback finishes for gapless playback
  void PrepareNextUrl(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 beginning_nanosec, const qint64 end_nanosec);
  void SetNextUrl();
  bool has_next_valid_url() const { return next_stream_url_.isValid(); }

  void SetSourceDevice(const QString &device) { source_device_ = device; }

  // Get information about the music playback
  QUrl media_url() const { return media_url_; }
  QUrl stream_url() const { return stream_url_; }
  double ebur128_loudness_normalizing_gain_db() const { return ebur128_loudness_normalizing_gain_db_; }
  QByteArray gst_url() const { return gst_url_; }
  QUrl next_media_url() const { return next_media_url_; }
  QUrl next_stream_url() const { return next_stream_url_; }
  QByteArray next_gst_url() const { return next_gst_url_; }
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
  void SetFaderVolume(const qreal volume);

 signals:
  void Error(const int pipeline_id, const int domain, const int error_code, const QString &message, const QString &debug);

  void EndOfStreamReached(const int pipeline_id, const bool has_next_track);
  void MetadataFound(const int pipeline_id, const EngineMetadata &bundle);

  void VolumeChanged(const uint volume);
  void FaderFinished();

  void BufferingStarted();
  void BufferingProgress(const int percent);
  void BufferingFinished();

  void AboutToFinish();

 protected:
  void timerEvent(QTimerEvent*) override;

 private:
  static QString GstStateText(const GstState state);
  GstElement *CreateElement(const QString &factory_name, const QString &name, GstElement *bin, QString &error) const;
  bool InitAudioBin(QString &error);
  void SetupVolume(GstElement *element);

  // Static callbacks.  The GstEnginePipeline instance is passed in the last argument.
  static GstPadProbeReturn UpstreamEventsProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self);
  static GstPadProbeReturn BufferProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self);
  static GstPadProbeReturn PlaybinProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self);
  static void ElementAddedCallback(GstBin *bin, GstBin*, GstElement *element, gpointer self);
  static void PadAddedCallback(GstElement *element, GstPad *pad, gpointer self);
  static void SourceSetupCallback(GstElement *playbin, GstElement *source, gpointer self);
  static void NotifyVolumeCallback(GstElement *element, GParamSpec *param_spec, gpointer self);
  static void AboutToFinishCallback(GstPlayBin *playbin, gpointer self);
  static GstBusSyncReply BusSyncCallback(GstBus *bus, GstMessage *msg, gpointer self);
  static gboolean BusWatchCallback(GstBus *bus, GstMessage *msg, gpointer self);
  static void TaskEnterCallback(GstTask *task, GThread *thread, gpointer self);

  void TagMessageReceived(GstMessage *msg);
  void ErrorMessageReceived(GstMessage *msg);
  void ElementMessageReceived(GstMessage *msg);
  void StateChangedMessageReceived(GstMessage *msg);
  void BufferingMessageReceived(GstMessage *msg);
  void StreamStatusMessageReceived(GstMessage *msg);
  void StreamStartMessageReceived();

  static QString ParseStrTag(GstTagList *list, const char *tag);
  static guint ParseUIntTag(GstTagList *list, const char *tag);

  void UpdateEBUR128LoudnessNormalizingGaindB();
  void UpdateStereoBalance();
  void UpdateEqualizer();

 private slots:
  void FaderTimelineFinished();

 private:
  static const int kGstStateTimeoutNanosecs;
  static const int kFaderFudgeMsec;
  static const int kEqBandCount;
  static const int kEqBandFrequencies[];

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
  bool fading_enabled_;

  // Stereo balance:
  // From -1.0 - 1.0
  // -1.0 is left, 1.0 is right.
  float stereo_balance_;

  // Equalizer
  int eq_preamp_;
  QList<int> eq_band_gains_;

  // ReplayGain
  int rg_mode_;
  double rg_preamp_;
  double rg_fallbackgain_;
  bool rg_compression_;

  // EBU R 128 Loudness Normalization
  bool ebur128_loudness_normalization_;

  // Buffering
  quint64 buffer_duration_nanosec_;
  double buffer_low_watermark_;
  double buffer_high_watermark_;
  bool buffering_;

  // Proxy
  QString proxy_address_;
  bool proxy_authentication_;
  QString proxy_user_;
  QString proxy_pass_;

  // Channels
  bool channels_enabled_;
  int channels_;

  // Options
  bool bs2b_enabled_;
  bool strict_ssl_enabled_;

  // These get called when there is a new audio buffer available
  QList<GstBufferConsumer*> buffer_consumers_;
  QMutex buffer_consumers_mutex_;
  qint64 segment_start_;
  bool segment_start_received_;

  // The URL that is currently playing, and the URL that is to be preloaded when the current track is close to finishing.
  QUrl media_url_;
  QUrl stream_url_;
  QByteArray gst_url_;
  QUrl next_media_url_;
  QUrl next_stream_url_;
  QByteArray next_gst_url_;

  // If this is > 0 then the pipeline will be forced to stop when playback goes past this position.
  qint64 end_offset_nanosec_;

  // We store the beginning and end for the preloading song too, so we can just carry on without reloading the file if the sections carry on from each other.
  qint64 next_beginning_offset_nanosec_;
  qint64 next_end_offset_nanosec_;

  // Set temporarily when moving to the next contiguous section in a multipart file.
  bool ignore_next_seek_;

  // Set temporarily when switching out the decode bin, so metadata doesn't get sent while the Player still thinks it's playing the last song
  bool ignore_tags_;

  // When the gstreamer source requests a redirect we store the URL here and callers can pick it up after the state change to PLAYING fails.
  QByteArray redirect_url_;

  // When we need to specify the device to use as source (for CD device)
  QString source_device_;

  // Seeking while the pipeline is in the READY state doesn't work, so we have to wait until it goes to PAUSED or PLAYING.
  // Also, we have to wait for the playbin to be connected.
  bool pipeline_is_active_;
  bool pipeline_is_connected_;
  qint64 pending_seek_nanosec_;

  // We can only use gst_element_query_position() when the pipeline is in
  // PAUSED nor PLAYING state. Whenever we get a new position (e.g. after a correct call to gst_element_query_position() or after a seek), we store
  // it here so that we can use it when using gst_element_query_position() is not possible.
  mutable gint64 last_known_position_ns_;

  // Complete the transition to the next song when it starts playing
  bool next_uri_set_;
  bool next_uri_reset_;

  double ebur128_loudness_normalizing_gain_db_;
  bool volume_set_;
  gdouble volume_internal_;
  uint volume_percent_;

  SharedPtr<QTimeLine> fader_;
  QBasicTimer fader_fudge_timer_;
  bool use_fudge_timer_;

  GstElement *pipeline_;
  GstElement *audiobin_;
  GstElement *audiosink_;
  GstElement *audioqueue_;
  GstElement *audioqueueconverter_;
  GstElement *volume_;
  GstElement *volume_sw_;
  GstElement *volume_fading_;
  GstElement *volume_ebur128_;
  GstElement *audiopanorama_;
  GstElement *equalizer_;
  GstElement *equalizer_preamp_;
  GstElement *eventprobe_;

  gulong upstream_events_probe_cb_id_;
  gulong buffer_probe_cb_id_;
  gulong playbin_probe_cb_id_;
  glong element_added_cb_id_;
  glong pad_added_cb_id_;
  glong notify_source_cb_id_;
  glong about_to_finish_cb_id_;
  glong notify_volume_cb_id_;

  QThreadPool set_state_threadpool_;

  GstSegment last_playbin_segment_{};

  bool logged_unsupported_analyzer_format_;

  bool about_to_finish_;

};

#endif  // GSTENGINEPIPELINE_H
