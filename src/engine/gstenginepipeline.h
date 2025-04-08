/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <optional>

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
#include <QList>
#include <QByteArray>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include "includes/shared_ptr.h"
#include "includes/mutex_protected.h"
#include "core/enginemetadata.h"

class QTimer;
class GstBufferConsumer;
struct GstPlayBin;

class GstEnginePipeline : public QObject {
  Q_OBJECT

 public:
  explicit GstEnginePipeline(QObject *parent = nullptr);
  ~GstEnginePipeline() override;

  // Globally unique across all pipelines.
  int id() const { return id_.value(); }

  // Call these setters before Init
  void set_output_device(const QString &output, const QVariant &device);
  void set_playbin3_enabled(const bool playbin3_enabled);
  void set_exclusive_mode(const bool exclusive_mode);
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
#ifdef HAVE_SPOTIFY
  void set_spotify_access_token(const QString &spotify_access_token);
#endif

  bool Finish();

  // Creates the pipeline, returns false on error
  bool InitFromUrl(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 beginning_offset_nanosec, const qint64 end_offset_nanosec, const double ebur128_loudness_normalizing_gain_db, QString &error);

  // GstBufferConsumers get fed audio data.  Thread-safe.
  void AddBufferConsumer(GstBufferConsumer *consumer);
  void RemoveBufferConsumer(GstBufferConsumer *consumer);
  void RemoveAllBufferConsumers();

  // Control the music playback
  Q_INVOKABLE QFuture<GstStateChangeReturn> SetState(const GstState state);
  Q_INVOKABLE QFuture<GstStateChangeReturn> Play(const bool pause, const quint64 offset_nanosec);
  Q_INVOKABLE bool Seek(const qint64 nanosec);
  void SeekAsync(const qint64 nanosec);
  void SeekDelayed(const qint64 nanosec);

  void SetVolume(const uint volume_percent);
  void SetStereoBalance(const float value);
  void SetEqualizerParams(const int preamp, const QList<int> &band_gains);
  void SetEBUR128LoudnessNormalizingGain_dB(const double ebur128_loudness_normalizing_gain_db);

  // If this is set then it will be loaded automatically when playback finishes for gapless playback
  bool HasNextUrl() const;
  bool HasMatchingNextUrl() const;
  void PrepareNextUrl(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 beginning_offset_nanosec, const qint64 end_offset_nanosec);
  void SetNextUrl();

  void SetSourceDevice(const QString &device);

  void StartFader(const qint64 duration_nanosec, const QTimeLine::Direction direction = QTimeLine::Forward, const QEasingCurve::Type shape = QEasingCurve::Linear, const bool use_fudge_timer = true);

  // Get information about the music playback
  QUrl media_url() const { return media_url_; }
  QUrl stream_url() const { return stream_url_; }
  QByteArray gst_url() const { return gst_url_; }
  QMutex *mutex_url() const { return &mutex_url_; }
  QUrl next_media_url() const { return next_media_url_; }
  QUrl next_stream_url() const { return next_stream_url_; }
  QByteArray next_gst_url() const { return next_gst_url_; }
  QMutex *mutex_next_url() const { return &mutex_next_url_; }
  double ebur128_loudness_normalizing_gain_db() const { return ebur128_loudness_normalizing_gain_db_; }

  // Returns this pipeline's state. May return GST_STATE_NULL if the state check timed out. The timeout value is a reasonable default.
  GstState state() const;
  // Please note that this method (unlike GstEngine's.length()) is multiple-section media unaware.
  qint64 length() const;
  // Please note that this method (unlike GstEngine's.position()) is multiple-section media unaware.
  qint64 position() const;
  qint64 segment_start() const { return segment_start_.value(); }

  // Don't allow the user to change the playback state (playing/paused) while the pipeline is buffering.
  bool is_buffering() const { return buffering_.value(); }

  bool exclusive_mode() const { return exclusive_mode_; }

  QByteArray redirect_url() const { return redirect_url_; }
  QMutex *mutex_redirect_url() { return &mutex_redirect_url_; }

  QString source_device() const { return source_device_; }

 Q_SIGNALS:
  void SetStateFinished(const GstStateChangeReturn state_change_return);
  void Error(const int pipeline_id, const int domain, const int error_code, const QString &message, const QString &debug);
  void EndOfStreamReached(const int pipeline_id, const bool has_next_track);
  void MetadataFound(const int pipeline_id, const EngineMetadata &bundle);
  void AboutToFinish();
  void Finished();
  void VolumeChanged(const uint volume);
  void FaderFinished(const int pipeline_id);
  void BufferingStarted();
  void BufferingProgress(const int percent);
  void BufferingFinished();

 private:
  static QString GstStateText(const GstState state);
  GstElement *CreateElement(const QString &factory_name, const QString &name, GstElement *bin, QString &error) const;
  bool IsStateNull() const;
  bool InitAudioBin(QString &error);
  void SetupVolume(GstElement *element);
  void SetStateAsync(const GstState state);

  // Static callbacks.  The GstEnginePipeline instance is passed in the last argument.
  static GstPadProbeReturn UpstreamEventsProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self);
  static GstPadProbeReturn BufferProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self);
  static GstPadProbeReturn PadProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self);
  static void ElementAddedCallback(GstBin *bin, GstBin *sub_bin, GstElement *element, gpointer self);
  static void ElementRemovedCallback(GstBin *bin, GstBin *sub_bin, GstElement *element, gpointer self);
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

  void Disconnect();
  void ResumeFaderAsync();

  void ProcessPendingSeek(const GstState state);

 private Q_SLOTS:
  void SetStateAsyncSlot(const GstState state);
  void SetStateFinishedSlot(const GstState state, const GstStateChangeReturn state_change_return);
  void SetFaderVolume(const qreal volume);
  void FaderTimelineStateChanged(const QTimeLine::State state);
  void FaderTimelineFinished();
  void FaderTimelineTimeout();
  void FaderFudgeFinished();

 private:
  // Using == to compare two pipelines is a bad idea, because new ones often get created in the same address as old ones.  This ID will be unique for each pipeline.
  // Threading warning: access to the static ID field isn't protected by a mutex because all pipeline creation is currently done in the main thread.
  static int sId;
  mutex_protected<int> id_;

  QThreadPool set_state_threadpool_;

  bool playbin3_support_;
  bool volume_full_range_support_;

  bool playbin3_enabled_;

  // General settings for the pipeline
  QString output_;
  QVariant device_;
  bool exclusive_mode_;
  bool volume_enabled_;
  bool fading_enabled_;
  mutex_protected<bool> strict_ssl_enabled_;

  // Buffering
  quint64 buffer_duration_nanosec_;
  double buffer_low_watermark_;
  double buffer_high_watermark_;

  // Proxy
  QString proxy_address_;
  bool proxy_authentication_;
  QString proxy_user_;
  QString proxy_pass_;
  QMutex mutex_proxy_;

  // Channels
  bool channels_enabled_;
  int channels_;

  // bs2b
  bool bs2b_enabled_;

  // Stereo balance:
  // From -1.0 - 1.0
  // -1.0 is left, 1.0 is right.
  bool stereo_balancer_enabled_;
  float stereo_balance_;

  // Equalizer
  bool eq_enabled_;
  int eq_preamp_;
  QList<int> eq_band_gains_;

  // ReplayGain
  bool rg_enabled_;
  int rg_mode_;
  double rg_preamp_;
  double rg_fallbackgain_;
  bool rg_compression_;

  // EBU R 128 Loudness Normalization
  bool ebur128_loudness_normalization_;

  // Spotify
#ifdef HAVE_SPOTIFY
  QString spotify_access_token_;
  mutable QMutex mutex_spotify_access_token_;
#endif

  // The URL that is currently playing, and the URL that is to be preloaded when the current track is close to finishing.
  QUrl media_url_;
  QUrl stream_url_;
  QByteArray gst_url_;
  mutable QMutex mutex_url_;

  QUrl next_media_url_;
  QUrl next_stream_url_;
  QByteArray next_gst_url_;
  mutable QMutex mutex_next_url_;

  double ebur128_loudness_normalizing_gain_db_;

  // These get called when there is a new audio buffer available
  QList<GstBufferConsumer*> buffer_consumers_;
  QMutex mutex_buffer_consumers_;

  mutex_protected<qint64> segment_start_;
  mutex_protected<bool> segment_start_received_;
  GstSegment last_playbin_segment_{};

  mutex_protected<qint64> beginning_offset_nanosec_;
  // If this is > 0 then the pipeline will be forced to stop when playback goes past this position.
  mutex_protected<qint64> end_offset_nanosec_;

  // We store the beginning and end for the preloading song too, so we can just carry on without reloading the file if the sections carry on from each other.
  mutex_protected<qint64> next_beginning_offset_nanosec_;
  mutex_protected<qint64> next_end_offset_nanosec_;

  // Set temporarily when moving to the next contiguous section in a multipart file.
  mutex_protected<bool> ignore_next_seek_;

  // Set temporarily when switching out the decode bin, so metadata doesn't get sent while the Player still thinks it's playing the last song
  mutex_protected<bool> ignore_tags_;

  // When the gstreamer source requests a redirect we store the URL here and callers can pick it up after the state change to PLAYING fails.
  mutable QMutex mutex_redirect_url_;
  QByteArray redirect_url_;

  // When we need to specify the device to use as source (for CD device)
  QString source_device_;
  QMutex mutex_source_device_;

  // Seeking while the pipeline is in the READY state doesn't work, so we have to wait until it goes to PAUSED or PLAYING.
  // Also, we have to wait for the playbin to be connected.
  mutex_protected<bool> pipeline_connected_;
  mutex_protected<bool> pipeline_active_;
  mutex_protected<bool> buffering_;

  mutex_protected<GstState> pending_state_;
  mutex_protected<qint64> pending_seek_nanosec_;
  mutex_protected<GstState> pending_seek_ready_previous_state_;

  // We can only use gst_element_query_position() when the pipeline is in
  // PAUSED nor PLAYING state. Whenever we get a new position (e.g. after a correct call to gst_element_query_position() or after a seek), we store
  // it here so that we can use it when using gst_element_query_position() is not possible.
  mutable gint64 last_known_position_ns_;

  // Complete the transition to the next song when it starts playing
  mutex_protected<bool> next_uri_set_;
  mutex_protected<bool> next_uri_need_reset_;
  mutex_protected<bool> next_uri_reset_;

  mutex_protected<bool> volume_set_;
  mutex_protected<gdouble> volume_internal_;
  mutex_protected<uint> volume_percent_;

  mutex_protected<bool> fader_active_;
  mutex_protected<bool> fader_running_;
  bool fader_use_fudge_timer_;
  SharedPtr<QTimeLine> fader_;
  QTimer *timer_fader_fudge_;
  QTimer *timer_fader_timeout_;

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

  std::optional<gulong> upstream_events_probe_cb_id_;
  std::optional<gulong> buffer_probe_cb_id_;
  std::optional<gulong> pad_probe_cb_id_;
  std::optional<gulong> element_added_cb_id_;
  std::optional<gulong> element_removed_cb_id_;
  std::optional<gulong> pad_added_cb_id_;
  std::optional<gulong> notify_source_cb_id_;
  std::optional<gulong> about_to_finish_cb_id_;
  std::optional<gulong> notify_volume_cb_id_;

  bool logged_unsupported_analyzer_format_;
  mutex_protected<bool> about_to_finish_;
  mutex_protected<bool> finish_requested_;
  mutex_protected<bool> finished_;

  mutex_protected<int> set_state_in_progress_;
  mutex_protected<int> set_state_async_in_progress_;

  mutex_protected<GstState> last_set_state_in_progress_;
  mutex_protected<GstState> last_set_state_async_in_progress_;
};

using GstEnginePipelinePtr = QSharedPointer<GstEnginePipeline>;

#endif  // GSTENGINEPIPELINE_H
