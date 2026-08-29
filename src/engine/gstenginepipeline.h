/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <atomic>
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
#include "core/enginemetadata.h"

class GstBufferConsumer;
struct GstPlayBin;

class GstEnginePipeline : public QObject {
  Q_OBJECT

 public:
  explicit GstEnginePipeline(QObject *parent = nullptr);
  ~GstEnginePipeline() override;

  bool event(QEvent *e) override;

  // Globally unique across all pipelines.
  int id() const { return id_.load(); }

  // Call these setters before Init
  void set_output_device(const QString &output, const QVariant &device);
  void set_playbin3_enabled(const bool playbin3_enabled);
  void set_exclusive_mode(const bool exclusive_mode);
  void set_volume_enabled(const bool enabled);
  void set_volume_exponential(const bool enabled);
  void set_stereo_balancer_enabled(const bool enabled);
  void set_equalizer_enabled(const bool enabled);
  void set_replaygain(const bool enabled, const int mode, const double preamp, const double fallbackgain, const bool compression);
  void set_ebur128_loudness_normalization(const bool enabled);
  void set_buffer_duration_nanosec(const quint64 duration_nanosec);
  void set_buffer_low_watermark(const double value);
  void set_buffer_high_watermark(const double value);
  void set_device_warmup_duration_ms(const int duration_ms);
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

  void SetSourceDevice(const QString &device);

  void StartFader(const qint64 duration_nanosec, const QTimeLine::Direction direction, const QEasingCurve::Type shape, const bool use_fudge_timer);

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
  qint64 segment_start() const { return segment_start_.load(); }

  // Don't allow the user to change the playback state (playing/paused) while the pipeline is buffering.
  bool is_buffering() const { return buffering_.load(); }

  bool exclusive_mode() const { return exclusive_mode_; }

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
  bool StateChangeInProgress();
  bool InitAudioBin(QString &error);
  void SetupVolume(GstElement *element);
  void ReapplyVolume();
  double PercentToInternalVolume(const uint volume_percent) const;
  uint InternalVolumeToPercent(const double volume_internal) const;
  void SetStateAsync(const GstState state);
  // Applies a state without counting as a newer request, so it does not invalidate async requests queued in the meantime (see SetState() and SetStateAsyncSlot()).
  QFuture<GstStateChangeReturn> ApplyState(const GstState state);
  void StartPlaybackAfterWarmup();
  void EmitFinishedIfQuiescent();
  void SetNextUrl();

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

  void HandleBusMessage(GstMessage *msg);
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

  void DisconnectCallbacks();
  void ResumeFaderAsync();
  // Clears the fading state on behalf of the fade identified by fader_generation, and does nothing if that fade has since been replaced.
  void ClearFaderState(const quint64 fader_generation);
  // Arms the timeout for the fade identified by fader_generation. Both are taken from the same critical section by the caller, so the interval always belongs to the fade the generation names.
  void StartFaderTimeout(const quint64 fader_generation, const qint64 interval_msec);
  // Runs when a fade takes longer than expected. Not a slot: it is scheduled as a functor so each timeout carries the generation of the fade it was armed for (see StartFaderTimeout()).
  void FaderTimelineTimeout(const quint64 fader_generation);
  // Runs once the fudge delay after a fade has elapsed, and emits FaderFinished() unless the fade it was armed for has since been replaced or torn down. Scheduled as a functor for the same reason as FaderTimelineTimeout().
  void FaderFudgeFinished(const quint64 fader_generation);

  void ProcessPendingSeek(const GstState state);
  // Handles a request queued by SetStateAsync(). Not a slot: SetStateAsync() posts it as a functor, so the call is checked at compile time instead of resolved by name at runtime.
  void SetStateAsyncSlot(const GstState state, const quint64 state_request_generation);

 private Q_SLOTS:
  void SetStateFinishedSlot(const GstState state, const GstStateChangeReturn state_change_return);
  void SetFaderVolume(const qreal volume);
  void FaderTimelineStateChanged(const QTimeLine::State state);
  void FaderTimelineFinished();

 private:
  // Using == to compare two pipelines is a bad idea, because new ones often get created in the same address as old ones.  This ID will be unique for each pipeline.
  static std::atomic<int> sId;
  std::atomic<int> id_;

  // Separate shared thread pool for manufactured-EOS pad sends (see ErrorMessageReceived()).
  // These can block for as long as the current track takes to finish draining, so they must not share the per-pipeline state-change thread pool with gst_element_set_state() calls, which are expected to complete quickly and whose timely completion drives playback control (pause/stop/seek); sharing would let a long-blocked pad send starve state changes.
  static QThreadPool *shared_pad_send_threadpool();

  bool playbin3_support_;
  bool volume_full_range_support_;

  bool playbin3_enabled_;

  // General settings for the pipeline
  QString output_;
  QVariant device_;
  bool exclusive_mode_;
  bool volume_enabled_;
  bool volume_exponential_;
  bool fading_enabled_;
  std::atomic<bool> strict_ssl_enabled_;

  // Buffering
  quint64 buffer_duration_nanosec_;
  double buffer_low_watermark_;
  double buffer_high_watermark_;

  // Audio device (DAC) warm-up delay in milliseconds inserted between preroll (PAUSED) and playback (PLAYING); 0 disables it.
  int device_warmup_duration_ms_;
  // Set for a fresh non-paused start with a warm-up delay configured, so the first time the pipeline reaches PAUSED we wait before going to PLAYING.  One-shot per Play().
  std::atomic<bool> device_warmup_pending_;
  // Bumped by every SetState() call so a scheduled warm-up timer can detect that a newer transition (e.g. the user paused/stopped, or a newer start) superseded it and skip resuming playback.
  std::atomic<quint64> device_warmup_generation_;

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

  std::atomic<qint64> segment_start_;
  std::atomic<bool> segment_start_received_;
  GstSegment last_playbin_segment_{};

  std::atomic<qint64> beginning_offset_nanosec_;
  // If this is > 0 then the pipeline will be forced to stop when playback goes past this position.
  std::atomic<qint64> end_offset_nanosec_;

  // We store the beginning and end for the preloading song too, so we can just carry on without reloading the file if the sections carry on from each other.
  std::atomic<qint64> next_beginning_offset_nanosec_;
  std::atomic<qint64> next_end_offset_nanosec_;

  // Set temporarily when moving to the next contiguous section in a multipart file.
  std::atomic<bool> ignore_next_seek_;

  // Set temporarily when switching out the decode bin, so metadata doesn't get sent while the Player still thinks it's playing the last song
  std::atomic<bool> ignore_tags_;

  // When we need to specify the device to use as source (for CD device)
  QString source_device_;
  QMutex mutex_source_device_;

  // Seeking while the pipeline is in the READY state doesn't work, so we have to wait until it goes to PAUSED or PLAYING.
  // Also, we have to wait for the playbin to be connected.
  std::atomic<bool> pipeline_connected_;
  std::atomic<bool> pipeline_active_;
  std::atomic<bool> buffering_;

  // Last pipeline state observed from GST_MESSAGE_STATE_CHANGED,
  // so state() can be answered without a blocking gst_element_get_state() call (which would stall the GUI thread when called from e.g. AnalyzerBase::paintEvent during a pipeline state change).
  std::atomic<GstState> current_state_;

  std::atomic<GstState> pending_state_;
  std::atomic<qint64> pending_seek_nanosec_;
  std::atomic<GstState> pending_seek_ready_previous_state_;

  // We can only use gst_element_query_position() when the pipeline is in PAUSED nor PLAYING state.
  // Whenever we get a new position (e.g. after a correct call to gst_element_query_position() or after a seek), we store it here so that we can use it when using gst_element_query_position() is not possible.
  mutable std::atomic<gint64> last_known_position_ns_;

  // Complete the transition to the next song when it starts playing
  std::atomic<bool> next_uri_set_;
  std::atomic<bool> next_uri_need_reset_;
  std::atomic<bool> next_uri_reset_;

  // Guards ErrorMessageReceived()'s manufactured-EOS path: reset to false each time a new next-URI is set (SetNextUrl()), and claimed with a single compare_exchange so repeated error messages about the same failed next-URI (e.g. from multiple internal elements) submit at most one manufactured EOS per next-URI cycle.
  std::atomic<bool> next_uri_eos_manufactured_;

  // volume_set_, volume_internal_ and volume_percent_ are read independently in many places, but updates that mutate two or more together must hold mutex_volume_.
  mutable QMutex mutex_volume_;
  std::atomic<bool> volume_set_;
  std::atomic<gdouble> volume_internal_;
  std::atomic<uint> volume_percent_;

  std::atomic<bool> fader_active_;
  std::atomic<bool> fader_running_;
  bool fader_use_fudge_timer_;
  SharedPtr<QTimeLine> fader_;
  // Identifies the current fade. Bumped whenever a fade is started, re-armed or cleared, and captured by the timeout scheduled for that fade, so a timeout belonging to a fade that has since been replaced or cleared can be told apart from the one belonging to the fade that is running now.
  quint64 fader_generation_;
  // Interval the current fade's timeout is armed with, kept so ResumeFaderAsync() can re-arm the same timeout without recomputing it from the fade duration.
  qint64 fader_timeout_interval_msec_;
  mutable QMutex mutex_fader_;

  GstElement *pipeline_;
  GstElement *audiobin_;
  GstElement *audiosink_;
  GstElement *audioqueue_;
  GstElement *volume_;
  GstElement *volume_sw_;
  GstElement *volume_fading_;
  GstElement *volume_ebur128_;
  GstElement *audiopanorama_;
  GstElement *equalizer_;
  GstElement *equalizer_preamp_;
  GstElement *eventprobe_;
  GstElement *bufferprobe_;

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
  std::atomic<bool> about_to_finish_;
  std::atomic<bool> finish_requested_;
  std::atomic<bool> finished_;

  // Identifies the current bus-watch session. Bumped by DisconnectCallbacks() so that GstBusMessageEvents posted from the GLib thread before teardown (Windows/macOS) are dropped instead of handled after the watch is gone or replaced.
  std::atomic<quint64> bus_message_generation_;

  // Per-pipeline thread pool for gst_element_set_state() calls, limited to one thread so that no two state changes run concurrently on this pipeline and queued tasks (submitted via SetStateAsync) run in submission order.
  // It is per-pipeline rather than shared so that a slow downwards transition on one pipeline (going to NULL can block until the streaming threads have stopped) cannot hold up another pipeline's state changes.
  QThreadPool state_threadpool_;

  // Number of SetStateAsync() requests that have been queued but not yet turned into a running state change.
  // Incremented (possibly from a GStreamer streaming thread) the moment a request is queued and decremented when its slot runs, so that a state change is never briefly invisible while handing off from the queue to a pending future.
  // A non-zero value seen by a running SetStateAsyncSlot() also means newer requests are already queued behind it, so the one it is handling has been superseded before it was ever applied.
  std::atomic<int> set_state_async_in_progress_;

  // Bumped by every SetState() call, so an async request that snapshotted an older value when it was queued can tell that a newer state change was requested while it sat in the queue, and drop itself instead of resurrecting stale state.
  // Only SetState() bumps it: a request applied from SetStateAsyncSlot() goes through ApplyState() instead, since anything queued after it is by definition newer and must still be applied.
  // Guarded by mutex_set_state_async_, so that reading it and queueing the request it is snapshotted for cannot be interleaved with a bump.
  quint64 set_state_request_generation_;
  QMutex mutex_set_state_async_;

  // Running gst_element_set_state() calls for this pipeline.
  // Doubles as the source of truth for "a synchronous state change is in flight" and lets the destructor wait for them before unreffing the pipeline.
  QList<QFuture<GstStateChangeReturn>> pending_state_changes_;
  mutable QMutex mutex_pending_state_changes_;

  // Running gst_pad_send_event() calls manufacturing an EOS for the "ignore error" gapless path in ErrorMessageReceived().
  // gst_pad_send_event() blocks on the pad's stream lock until buffers already queued ahead of it (the remainder of the still-playing current track) have drained, so it is run off the main thread to avoid freezing (or deadlocking) the UI while that drains.
  // Tracked here so the destructor can wait for it before unreffing the pipeline, exactly like pending_state_changes_.
  QList<QFuture<gboolean>> pending_pad_send_events_;
  mutable QMutex mutex_pending_pad_send_events_;
};

using GstEnginePipelinePtr = QSharedPointer<GstEnginePipeline>;

#endif  // GSTENGINEPIPELINE_H
