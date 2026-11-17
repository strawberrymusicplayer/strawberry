/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <atomic>
#include <memory>

#include "gtest_include.h"

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <QFile>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFuture>
#include <QList>
#include <QMutexLocker>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThreadPool>
#include <QTimer>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "constants/timeconstants.h"
#include "core/taskmanager.h"
#include "engine/enginebase.h"
#include "engine/gstengine.h"
#include "engine/gstenginepipeline.h"
#include "engine/gststartup.h"

#include "test_utils.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// Declared at file scope so that GstEngine's "friend class GstEngineTest;" resolves to this class.
class GstEngineTest {
 public:
  static void SetOutput(GstEngine &engine, const QString &output) { engine.output_ = output; }

  static void SetFadingEnabled(GstEngine &engine, const bool enabled) {
    engine.fadeout_enabled_ = enabled;
    engine.crossfade_enabled_ = enabled;
    engine.autocrossfade_enabled_ = enabled;
    engine.fadeout_pause_enabled_ = enabled;
  }

  static qsizetype FadingOutPipelines(const GstEngine &engine) { return engine.fadeout_pipelines_.count(); }
  static bool FadingOutToPause(const GstEngine &engine) { return engine.fadeout_pause_pipeline_ != nullptr; }
  static GstDiscoverer *Discoverer(const GstEngine &engine) { return engine.discoverer_; }
  // Sets the state like a state changed message does, the test file would finish playing before it could be paused.
  static void SetPipelinePlaying(GstEngine &engine) { engine.current_pipeline_->current_state_ = GST_STATE_PLAYING; }
  static void SetPipelineState(GstEngine &engine, const GstState state) { engine.current_pipeline_->current_state_ = state; }

  // The pipeline is not playing in the tests, so the seek position is stored as its pending seek, -1 when there is none.
  static qint64 PendingSeek(const GstEngine &engine) { return engine.current_pipeline_->pending_seek_nanosec_.load(); }

  // Moves the pipeline to the next track like gapless playback does, when the next stream starts.
  static void StartNextTrack(GstEngine &engine, const QUrl &url) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    pipeline->PrepareNextUrl(url, url, url.toEncoded(), 0, 0);
    pipeline->next_uri_set_ = true;
    pipeline->StreamStartMessageReceived();
  }

  static int TrackChanges(const GstEngine &engine) { return engine.current_pipeline_->track_changes(); }
  // Counts a track change like the next stream starting does.
  static void ChangeTrack(GstEngine &engine) { ++engine.current_pipeline_->track_changes_; }
  static void SetIgnoreNextSeek(GstEngine &engine) { engine.current_pipeline_->ignore_next_seek_ = true; }
  // Takes the pending seek, like when the pad of the next pipeline was added on a streaming thread, it is queued with QueueTakenSeek().
  struct TakenSeek {
    qint64 nanosec;
    int expected_track_changes;
    int seek_generation;
  };
  static TakenSeek TakePendingSeek(GstEngine &engine, const qint64 nanosec) {
    engine.current_pipeline_->SetPendingSeek(nanosec, -1);
    TakenSeek taken_seek{};
    taken_seek.nanosec = engine.current_pipeline_->TakePendingSeek(&taken_seek.expected_track_changes, &taken_seek.seek_generation);
    return taken_seek;
  }
  static void QueueTakenSeek(GstEngine &engine, const TakenSeek &taken_seek) { engine.current_pipeline_->SeekAsync(taken_seek.nanosec, taken_seek.expected_track_changes, taken_seek.seek_generation); }
  static void SetPendingSeek(GstEngine &engine, const qint64 nanosec) { engine.current_pipeline_->SetPendingSeek(nanosec, -1); }
  static void QueueTakenSeekDelayed(GstEngine &engine, const TakenSeek &taken_seek) { engine.current_pipeline_->SeekDelayed(taken_seek.nanosec, taken_seek.expected_track_changes, taken_seek.seek_generation); }
  static bool PipelineSeek(GstEngine &engine, const qint64 nanosec, const int expected_track_changes) { return engine.current_pipeline_->Seek(nanosec, expected_track_changes); }
  // Retries the pending seek like when the pipeline became active, the retry is queued.
  static void ProcessPendingSeek(GstEngine &engine) { engine.current_pipeline_->ProcessPendingSeek(GST_STATE_PAUSED); }
  // Like after the next URL was reverted for the seek, the retry is delayed.
  static void ProcessPendingSeekAfterNextUrlReset(GstEngine &engine) {
    engine.current_pipeline_->next_uri_reset_ = true;
    engine.current_pipeline_->pending_seek_ready_previous_state_ = GST_STATE_PAUSED;
    engine.current_pipeline_->ProcessPendingSeek(GST_STATE_PAUSED);
  }
  // Sets the pipeline up like a seek did while the next URL was set for gapless playback, the seek is pending until the next URL is reverted when the pipeline is in READY.
  static void SeekWhileNextUrlIsSet(GstEngine &engine, const QUrl &url, const qint64 nanosec, const GstState previous_state) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    pipeline->PrepareNextUrl(url, url, url.toEncoded(), 0, 0);
    pipeline->next_uri_set_ = true;
    ASSERT_TRUE(pipeline->Seek(nanosec, pipeline->track_changes()));
    ASSERT_TRUE(pipeline->next_uri_need_reset_.load());
    // The pipeline is not playing in the tests, so the state from before the seek is set like it was.
    pipeline->pending_seek_ready_previous_state_ = previous_state;
  }

  // The test track is short, so the pipeline is about to finish when it prerolled again after the next uri was reverted, and sets the prepared next uri again, then the seek after the reset is reset again, clearing the prepared next uri avoids that.
  static void ClearNextUrl(GstEngine &engine) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    QMutexLocker l(&pipeline->mutex_next_url_);
    pipeline->next_media_url_.clear();
    pipeline->next_stream_url_.clear();
    pipeline->next_gst_url_.clear();
  }

  // Requests a state like pausing does while the pipeline is going to READY.
  static void RequestState(GstEngine &engine, const GstState state) { engine.current_pipeline_->SetState(state); }
  // Starts the next track from an offset, like for a CUE section.
  static void PlayFromOffset(GstEngine &engine, const quint64 offset_nanosec, const bool pause = false) { engine.current_pipeline_->Play(pause, offset_nanosec); }
  static void SetDeviceWarmup(GstEngine &engine, const int duration_ms) { engine.current_pipeline_->set_device_warmup_duration_ms(duration_ms, duration_ms); }
  // Skips the warm-up for starting playback, like for a pipeline created while another pipeline is active.
  static void SetDeviceWarmupSkipped(GstEngine &engine, const int reopen_duration_ms) { engine.current_pipeline_->set_device_warmup_duration_ms(0, reopen_duration_ms); }
  static void SetDeviceWarmupWaiting(GstEngine &engine) { engine.current_pipeline_->device_warmup_waiting_ = true; }
  // Sets the state which is requested, like when a request is queued, but not applied yet.
  static void SetTargetState(GstEngine &engine, const GstState state) { engine.current_pipeline_->target_state_ = state; }
  static int DeviceWarmupDuration(const GstEngine &engine) { return engine.current_pipeline_->device_warmup_duration_ms_; }
  // Queues the pending seek like when a pad is added.
  static void QueuePendingSeek(GstEngine &engine) { engine.current_pipeline_->QueuePendingSeek(); }
  static bool DeviceWarmupPending(const GstEngine &engine) { return engine.current_pipeline_->device_warmup_pending_.load(); }

  // Sets the next URL like gapless playback does, before the next stream starts.
  static void SetNextUrl(GstEngine &engine, const QUrl &url) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    pipeline->PrepareNextUrl(url, url, url.toEncoded(), 0, 0);
    pipeline->next_uri_set_ = true;
  }
  static GstState SeekResetPreviousState(const GstEngine &engine) { return engine.current_pipeline_->pending_seek_ready_previous_state_.load(); }

  enum class SegmentSinkState {
    NotReached,
    NotPlaying,
    Playing,
  };
  // Whether the audio sink was already playing when the first segment with the expected start reached it.
  // After a flushing seek while paused, the sink only reaches PLAYING after the segment of the seek, so when it was playing, playback started before the seek.
  // Only the first matching segment is recorded, so a later segment can't replace the sink state of the segment of the seek.
  struct SegmentObservation {
    explicit SegmentObservation(const qint64 _expected_start) : expected_start(_expected_start) {}
    const qint64 expected_start;
    std::atomic<SegmentSinkState> sink_state{SegmentSinkState::NotReached};
  };
  // Records each segment which reaches the audio sink, like the segment after a seek, also when the track ends, and the pipeline is removed right after that.
  // The probe shares the observation with the test, the probe stays until the pipeline is removed, and can still run on a streaming thread after the test ended, also when the test fails early.
  static void ObserveSegments(GstEngine &engine, const SharedPtr<SegmentObservation> &segments) {
    GstPad *pad = gst_element_get_static_pad(engine.current_pipeline_->audiosink_, "sink");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, SegmentProbeCallback, new SharedPtr<SegmentObservation>(segments), SegmentProbeDestroy);
    gst_object_unref(pad);
  }
  static GstPadProbeReturn SegmentProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer data) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT) {
      const GstSegment *segment = nullptr;
      gst_event_parse_segment(event, &segment);
      GstState sink_state = GST_STATE_NULL;
      GstElement *sink = gst_pad_get_parent_element(pad);
      if (sink) {
        GST_OBJECT_LOCK(sink);
        sink_state = GST_STATE(sink);
        GST_OBJECT_UNLOCK(sink);
        gst_object_unref(sink);
      }
      const SharedPtr<SegmentObservation> &segments = *static_cast<SharedPtr<SegmentObservation>*>(data);
      if (static_cast<qint64>(segment->start) == segments->expected_start) {
        SegmentSinkState not_reached = SegmentSinkState::NotReached;
        segments->sink_state.compare_exchange_strong(not_reached, sink_state == GST_STATE_PLAYING ? SegmentSinkState::Playing : SegmentSinkState::NotPlaying);
      }
    }
    return GST_PAD_PROBE_OK;
  }
  static void SegmentProbeDestroy(gpointer data) { delete static_cast<SharedPtr<SegmentObservation>*>(data); }
  // Sets the uri of playbin, like when the next uri is set when the stream is about to finish.
  static void SetPlaybinUri(GstEngine &engine, const QUrl &url) { g_object_set(G_OBJECT(engine.current_pipeline_->pipeline_), "uri", url.toEncoded().constData(), nullptr); }
  // The uri which playbin opened.
  static QUrl PlaybinCurrentUri(const GstEngine &engine) {
    gchar *uri = nullptr;
    g_object_get(G_OBJECT(engine.current_pipeline_->pipeline_), "current-uri", &uri, nullptr);
    const QUrl url = uri ? QUrl::fromEncoded(QByteArray(uri)) : QUrl();
    g_free(uri);
    return url;
  }
  static QUrl PipelineStreamUrl(const GstEngine &engine) { return engine.current_pipeline_->stream_url(); }
  // Blocks the state changes of the pipeline until the semaphore is released, the state changes run one at a time, so the next state change waits for this.
  static void BlockStateChanges(GstEngine &engine, const SharedPtr<QSemaphore> &semaphore) {
    engine.current_pipeline_->state_threadpool_.start([semaphore]() { semaphore->acquire(); });
  }
  // Handles the stream start of the next stream, like when the next URL started before the pipeline went to READY.
  static void StartNextStream(GstEngine &engine) { engine.current_pipeline_->StreamStartMessageReceived(); }
  // Sets the start offset of the track, like for a CUE section.
  static void SetBeginningOffset(GstEngine &engine, const quint64 nanosec) { engine.beginning_offset_nanosec_ = nanosec; }
  // Starts the seek delay of the engine, like right after a seek, a seek of the engine then waits for it.
  static void StartSeekTimer(GstEngine &engine) { engine.seek_timer_->start(); }
  // Applies the seek which waited for the seek delay of the engine, like when the delay elapsed.
  static void SeekDelayElapsed(GstEngine &engine) { engine.SeekNow(); }
  // Sets up a new source, like when the pipeline prerolls for a newer state request, buffering is finished in a queued call.
  static void SourceSetupQueued(GstEngine &engine) {
    GstElement *source = gst_element_factory_make("fakesrc", nullptr);
    ASSERT_NE(nullptr, source);
    gst_object_ref_sink(source);
    GstEnginePipeline::SourceSetupCallback(engine.current_pipeline_->pipeline_, source, &*engine.current_pipeline_);
    gst_object_unref(source);
  }
  // Sets up a new source, then processes the queued calls, which finish buffering, but not the messages of the pipeline.
  static void SourceSetup(GstEngine &engine) {
    SourceSetupQueued(engine);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  }

  // Handles a state changed message of the pipeline going to READY.
  static void PipelineReady(GstEngine &engine) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    GstMessage *msg = gst_message_new_state_changed(GST_OBJECT(pipeline->pipeline_), GST_STATE_PAUSED, GST_STATE_READY, GST_STATE_VOID_PENDING);
    pipeline->StateChangedMessageReceived(msg);
    gst_message_unref(msg);
  }

  // Handles a state changed message of the pipeline going from READY to PAUSED.
  static void PipelinePaused(GstEngine &engine) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    GstMessage *msg = gst_message_new_state_changed(GST_OBJECT(pipeline->pipeline_), GST_STATE_READY, GST_STATE_PAUSED, GST_STATE_VOID_PENDING);
    pipeline->StateChangedMessageReceived(msg);
    gst_message_unref(msg);
  }

  // Handles a state changed message of the pipeline going down from PLAYING to PAUSED on the way to READY.
  static void PipelinePausedOnTheWayToReady(GstEngine &engine) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    GstMessage *msg = gst_message_new_state_changed(GST_OBJECT(pipeline->pipeline_), GST_STATE_PLAYING, GST_STATE_PAUSED, GST_STATE_READY);
    pipeline->StateChangedMessageReceived(msg);
    gst_message_unref(msg);
  }

  // Handles a state changed message of the pipeline going from PAUSED to PLAYING.
  static void PipelinePlaying(GstEngine &engine) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    GstMessage *msg = gst_message_new_state_changed(GST_OBJECT(pipeline->pipeline_), GST_STATE_PAUSED, GST_STATE_PLAYING, GST_STATE_VOID_PENDING);
    pipeline->StateChangedMessageReceived(msg);
    gst_message_unref(msg);
  }

  static void SetBuffering(GstEngine &engine, const bool buffering) { engine.current_pipeline_->buffering_ = buffering; }
  // Sets that the audio device warm-up elapsed while buffering, like when the warm-up timer elapsed while buffering.
  static void StartPlaybackAfterWarmup(GstEngine &engine) { engine.current_pipeline_->StartPlaybackAfterWarmup(); }
  // Queues a pause for buffering, like when buffering started while playing.
  static void BufferingPause(GstEngine &engine) { engine.current_pipeline_->SetStateAsync(GST_STATE_PAUSED, true); }
  // Waits for the state changes which are applied, without handling the state messages, and returns the state of the pipeline element.
  static GstState AppliedState(GstEngine &engine) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    QList<QFuture<GstStateChangeReturn>> pending_state_changes;
    {
      QMutexLocker locker(&pipeline->mutex_pending_state_changes_);
      pending_state_changes = pipeline->pending_state_changes_;
    }
    for (QFuture<GstStateChangeReturn> &future : pending_state_changes) {
      future.waitForFinished();
    }
    GstState state = GST_STATE_VOID_PENDING;
    gst_element_get_state(pipeline->pipeline_, &state, nullptr, 2 * GST_SECOND);
    return state;
  }
  static void SetDeviceWarmupElapsedWhileBuffering(GstEngine &engine) { engine.current_pipeline_->device_warmup_elapsed_while_buffering_ = true; }
  static bool Buffering(const GstEngine &engine) { return engine.current_pipeline_->buffering_.load(); }
  // Handles a message of the buffering queue with the percentage.
  static void BufferingMessage(GstEngine &engine, const int percent) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    GstMessage *msg = gst_message_new_buffering(GST_OBJECT(pipeline->audioqueue_), percent);
    pipeline->BufferingMessageReceived(msg);
    gst_message_unref(msg);
  }
  static void BufferingFinished(GstEngine &engine) { BufferingMessage(engine, 100); }
  static void SetPendingState(GstEngine &engine, const GstState state) { engine.current_pipeline_->pending_state_ = state; }
  // The pipeline is finished and removed when the track played to the end.
  static int QueuedSeeks(const GstEngine &engine) { return engine.current_pipeline_->queued_seeks_.load(); }
  static bool PendingStateFromBuffering(const GstEngine &engine) { return engine.current_pipeline_->pending_state_from_buffering_.load(); }
  static bool HasPipeline(const GstEngine &engine) { return engine.current_pipeline_ != nullptr; }
  static bool DeviceWarmupAfterSeek(const GstEngine &engine) { return engine.current_pipeline_->device_warmup_after_seek_.load(); }
  static bool PlayPending(const GstEngine &engine) { return engine.current_pipeline_->play_pending(); }
  static void Play(GstEngine &engine) { engine.current_pipeline_->Play(false, 0); }
  // Requests to start playback after the warm-up from a seek, like Play() with a seek requested while prerolling.
  static void SetSeekAfterDeviceWarmup(GstEngine &engine, const qint64 nanosec) {
    engine.current_pipeline_->SetPendingSeek(nanosec, -1);
    engine.current_pipeline_->device_warmup_pending_ = true;
  }
  static GstState PipelineState(const GstEngine &engine) { return engine.current_pipeline_->state(); }
  static qint64 LastKnownPosition(const GstEngine &engine) { return engine.current_pipeline_->last_known_position_ns_.load(); }
  // The position GStreamer reports, -1 when it is not known.
  static qint64 QueriedPosition(const GstEngine &engine) {
    gint64 position = -1;
    if (!gst_element_query_position(engine.current_pipeline_->pipeline_, GST_FORMAT_TIME, &position)) return -1;
    return position;
  }
  static GstState PendingState(const GstEngine &engine) { return engine.current_pipeline_->pending_state_.load(); }
  static bool DeviceWarmupWaiting(const GstEngine &engine) { return engine.current_pipeline_->device_warmup_waiting_.load(); }

  static bool NextUrlNeedsReset(const GstEngine &engine) { return engine.current_pipeline_->next_uri_need_reset_.load(); }
  static GstState TargetState(const GstEngine &engine) { return engine.current_pipeline_->target_state_.load(); }

  static void LockSeekSection(GstEngine &engine) { engine.current_pipeline_->mutex_seek_section_.lock(); }
  static void UnlockSeekSection(GstEngine &engine) { engine.current_pipeline_->mutex_seek_section_.unlock(); }

  // Passes a buffer which ends after the end of the section to the buffer probe, when the next section of the same file is queued, like playing a multipart file does.
  static void ProbeBufferAfterSectionEnd(GstEngine &engine, const QUrl &url, const qint64 section_end_nanosec) {
    GstEnginePipelinePtr pipeline = engine.current_pipeline_;
    pipeline->end_offset_nanosec_ = section_end_nanosec;
    pipeline->PrepareNextUrl(url, url, url.toEncoded(), section_end_nanosec, 0);
    GstPad *pad = gst_pad_new("src", GST_PAD_SRC);
    GstBuffer *buffer = gst_buffer_new();
    GST_BUFFER_TIMESTAMP(buffer) = static_cast<GstClockTime>(section_end_nanosec);
    GST_BUFFER_DURATION(buffer) = 100 * GST_MSECOND;
    GstPadProbeInfo info {};
    info.type = GST_PAD_PROBE_TYPE_BUFFER;
    info.data = buffer;
    GstEnginePipeline::BufferProbeCallback(pad, &info, &*pipeline);
    gst_buffer_unref(buffer);
    gst_object_unref(pad);
  }
};

namespace {

class GstEngineFadingTest : public ::testing::Test {
 protected:
  void SetUp() override {

    GstStartup::Initialize();

    ASSERT_TRUE(QFile::exists(resource_.fileName()));
    url_ = QUrl::fromLocalFile(resource_.fileName());

    engine_ = std::make_unique<GstEngine>(make_shared<TaskManager>());
    GstEngineTest::SetOutput(*engine_, u"fakesink"_s);
    GstEngineTest::SetFadingEnabled(*engine_, false);
    // A track ended when the pipeline was removed, not at a gapless transition to the next track, where the pipeline is kept.
    QObject::connect(&*engine_, &EngineBase::TrackEnded, &*engine_, [this]() {
      if (!GstEngineTest::HasPipeline(*engine_)) {
        track_ended_ = true;
      }
    });

  }

  // Stopping the discoverer while it's discovering can deadlock in GStreamer, so wait for it before the engine is deleted.
  void TearDown() override {
    WaitFor([this]() { return !discovering_; });
  }

  bool Load() {
    discovering_ = true;
    const bool success = engine_->Load(url_, url_, EngineBase::TrackChangeType::Manual, false, 0, 0, std::nullopt);
    GstDiscoverer *discoverer = GstEngineTest::Discoverer(*engine_);
    if (discoverer && !discovery_connected_) {
      discovery_connected_ = true;
      g_signal_connect(G_OBJECT(discoverer), "finished", G_CALLBACK(DiscoveryFinished), &discovering_);
    }
    return success;
  }

  static void DiscoveryFinished(GstDiscoverer *discoverer, gpointer self) {
    Q_UNUSED(discoverer)
    *static_cast<bool*>(self) = false;
  }

  // Processes Qt events and the default GLib context, which receives the discoverer signals, until the condition is true, returns false on timeout.
  template <typename Condition>
  static bool WaitFor(Condition condition) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
      if (timer.elapsed() > 10000) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      while (g_main_context_iteration(nullptr, FALSE)) {}
    }
    return true;
  }

  // Like WaitFor(), for a condition of the pipeline, which fails when the pipeline was removed, like after an error.
  template <typename Condition>
  bool WaitForPipeline(Condition condition) {
    return WaitFor([this, &condition]() { return !GstEngineTest::HasPipeline(*engine_) || condition(); }) && GstEngineTest::HasPipeline(*engine_);
  }

  // Processes events for the time, like the time a resume can be delayed by.
  static void ProcessEventsFor(const int msec) {
    QEventLoop event_loop;
    QTimer::singleShot(msec, &event_loop, &QEventLoop::quit);
    event_loop.exec();
  }

  // The pipeline is playing, or the track played to the end, and the pipeline was removed, a pipeline removed after an error is not counted.
  bool PlaybackStarted() const {
    if (GstEngineTest::HasPipeline(*engine_)) return GstEngineTest::TargetState(*engine_) == GST_STATE_PLAYING && GstEngineTest::PipelineState(*engine_) == GST_STATE_PLAYING;
    return track_ended_;
  }

  TemporaryResource resource_{u":/audio/strawberry.wav"_s};
  QUrl url_;
  bool track_ended_ = false;
  bool discovering_ = false;
  bool discovery_connected_ = false;
  std::unique_ptr<GstEngine> engine_;
};

// Fading out needs the pipeline to be built while fading was enabled.
TEST_F(GstEngineFadingTest, StopFadesOutPipelineWithFadingElement) {

  GstEngineTest::SetFadingEnabled(*engine_, true);
  ASSERT_TRUE(Load());

  engine_->Stop();

  EXPECT_EQ(1, GstEngineTest::FadingOutPipelines(*engine_));

}

// Regression test: with fading enabled after the pipeline was built, stopping played on at full volume for the fade duration and then cut off.
TEST_F(GstEngineFadingTest, StopDoesNotFadeOutPipelineWithoutFadingElement) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetFadingEnabled(*engine_, true);

  engine_->Stop();

  EXPECT_EQ(0, GstEngineTest::FadingOutPipelines(*engine_));

}

TEST_F(GstEngineFadingTest, CrossfadeFadesOutPipelineWithFadingElement) {

  GstEngineTest::SetFadingEnabled(*engine_, true);
  ASSERT_TRUE(Load());

  ASSERT_TRUE(Load());

  EXPECT_EQ(1, GstEngineTest::FadingOutPipelines(*engine_));

}

// Regression test: with crossfading enabled after the pipeline was built, the old track played on at full volume over the new track and then cut off.
TEST_F(GstEngineFadingTest, CrossfadeDoesNotFadeOutPipelineWithoutFadingElement) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetFadingEnabled(*engine_, true);

  ASSERT_TRUE(Load());

  EXPECT_EQ(0, GstEngineTest::FadingOutPipelines(*engine_));

}

TEST_F(GstEngineFadingTest, PauseFadesOutPipelineWithFadingElement) {

  GstEngineTest::SetFadingEnabled(*engine_, true);
  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelinePlaying(*engine_);

  engine_->Pause();

  EXPECT_TRUE(GstEngineTest::FadingOutToPause(*engine_));

}

// Regression test: with pause fading enabled after the pipeline was built, pausing played on at full volume for the fade duration and then cut off.
TEST_F(GstEngineFadingTest, PauseDoesNotFadeOutPipelineWithoutFadingElement) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::SetFadingEnabled(*engine_, true);

  engine_->Pause();

  EXPECT_FALSE(GstEngineTest::FadingOutToPause(*engine_));

}

class GstEngineSeekTest : public GstEngineFadingTest {
 protected:
  // Processes events for longer than the seek delay, so a waiting seek is applied.
  static void ProcessEventsForSeekDelay() {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 500) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
  }

  static constexpr qint64 kFirstSeekNanosec = 1000 * kNsecPerMsec;
  static constexpr qint64 kSecondSeekNanosec = 2000 * kNsecPerMsec;
  static constexpr qint64 kThirdSeekNanosec = 1500 * kNsecPerMsec;
};

// A seek right after another one waits, and is applied after the seek delay.
TEST_F(GstEngineSeekTest, WaitingSeekIsApplied) {

  ASSERT_TRUE(Load());

  engine_->Seek(kFirstSeekNanosec);
  ASSERT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));
  engine_->Seek(kSecondSeekNanosec);
  ASSERT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  ProcessEventsForSeekDelay();

  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: a seek waiting when the next track was loaded was applied to the next track.
TEST_F(GstEngineSeekTest, WaitingSeekIsNotAppliedToNextLoadedTrack) {

  ASSERT_TRUE(Load());
  engine_->Seek(kFirstSeekNanosec);
  engine_->Seek(kSecondSeekNanosec);

  ASSERT_TRUE(Load());
  ASSERT_EQ(-1, GstEngineTest::PendingSeek(*engine_));

  ProcessEventsForSeekDelay();

  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: the pipeline can move on to the next section of a multipart file on the streaming thread after the engine checked the track, the pipeline checks it again.
TEST_F(GstEngineSeekTest, PipelineSeekForPreviousTrackIsNotApplied) {

  ASSERT_TRUE(Load());
  const int track_changes = GstEngineTest::TrackChanges(*engine_);

  EXPECT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, track_changes + 1));
  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));

  EXPECT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, track_changes));
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// A pending seek is retried when the pipeline is ready, the pipeline is not active in the test, so the retry stores it as pending seek again.
TEST_F(GstEngineSeekTest, PendingSeekIsRetried) {

  ASSERT_TRUE(Load());
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  ASSERT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  GstEngineTest::ProcessPendingSeek(*engine_);
  ASSERT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  ProcessEventsForSeekDelay();

  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: the track the pending seek was for was not kept, so a retry after the pipeline moved on to the next track was applied to the next track.
TEST_F(GstEngineSeekTest, PendingSeekForPreviousTrackIsNotRetried) {

  ASSERT_TRUE(Load());
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  ASSERT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  GstEngineTest::StartNextTrack(*engine_, url_);
  GstEngineTest::ProcessPendingSeek(*engine_);
  ProcessEventsForSeekDelay();

  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: the same for the delayed retry after the next URL was reverted for the seek.
TEST_F(GstEngineSeekTest, PendingSeekForPreviousTrackIsNotRetriedAfterNextUrlReset) {

  ASSERT_TRUE(Load());
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  ASSERT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  GstEngineTest::StartNextTrack(*engine_, url_);
  GstEngineTest::ProcessPendingSeekAfterNextUrlReset(*engine_);
  ProcessEventsForSeekDelay();

  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: when the next stream started before the pipeline went to READY for a seek while the next URL was set, the next URL was not reverted, and the pipeline was not resumed either, so it stayed in READY.
TEST_F(GstEngineSeekTest, NextStreamStartedBeforeSeekResetResumesNextTrack) {

  ASSERT_TRUE(Load());
  // The next track is another file, so the test can tell which track is opened.
  TemporaryResource next_resource(u":/audio/strawberry.wav"_s);
  const QUrl next_url = QUrl::fromLocalFile(next_resource.fileName());
  ASSERT_NE(url_, next_url);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, next_url, kFirstSeekNanosec, GST_STATE_PAUSED);
  GstEngineTest::SetPlaybinUri(*engine_, next_url);
  const int track_changes = GstEngineTest::TrackChanges(*engine_);

  GstEngineTest::StartNextStream(*engine_);
  ASSERT_EQ(track_changes + 1, GstEngineTest::TrackChanges(*engine_));
  GstEngineTest::PipelineReady(*engine_);

  // The seek was for the previous track, the next track is played from the start, in the state from before the seek.
  EXPECT_FALSE(GstEngineTest::NextUrlNeedsReset(*engine_));
  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(next_url, GstEngineTest::PipelineStreamUrl(*engine_));

  // The pipeline prerolls the next track, the uri is not reverted to the previous track.
  EXPECT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && !GstEngineTest::PlaybinCurrentUri(*engine_).isEmpty(); }));
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_EQ(next_url, GstEngineTest::PlaybinCurrentUri(*engine_));

}

// Regression test: a pause requested while the pipeline went to READY was overridden by restoring the state from before the seek.
TEST_F(GstEngineSeekTest, PauseDuringSeekResetIsNotOverridden) {

  ASSERT_TRUE(Load());
  // Without the warm-up, restoring PLAYING goes to PLAYING right away.
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);

  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::PipelineReady(*engine_);

  EXPECT_FALSE(GstEngineTest::NextUrlNeedsReset(*engine_));
  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: the start offset of the next track requested while the pipeline went to READY, like for a CUE section, was dropped with the seek for the previous track.
TEST_F(GstEngineSeekTest, StartOffsetDuringSeekResetIsKept) {

  ASSERT_TRUE(Load());
  // Without the warm-up, restoring PLAYING goes to PLAYING right away.
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);

  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  GstEngineTest::PipelineReady(*engine_);

  EXPECT_FALSE(GstEngineTest::NextUrlNeedsReset(*engine_));
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: playback was resumed without the audio device warm-up, although going to READY closed the audio device.
TEST_F(GstEngineSeekTest, ResumeAfterSeekResetWaitsForDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);

  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);

  EXPECT_FALSE(GstEngineTest::NextUrlNeedsReset(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: a seek requested while the pipeline was prerolling for the warm-up was not applied before playback started, so playback started from the wrong position.
TEST_F(GstEngineSeekTest, SeekDuringWarmupPrerollIsApplied) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

  // The pipeline is not prerolled yet, so the seek is pending, the state changes are handled when the events are processed below.
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  ASSERT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  // The pipeline prerolls, the queued seek is applied, and the warm-up starts after it.
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::DeviceWarmupWaiting(*engine_); }));
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::PipelineState(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // Playback starts when the warm-up elapsed.
  // The track is short, so when it plays, it can play to the end, and the pipeline is removed.
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: when buffering lasted longer than the audio device warm-up, the warm-up started playback while still buffering.
TEST_F(GstEngineSeekTest, DeviceWarmupElapsedWhileBufferingWaitsForBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 100);
  GstEngineTest::Play(*engine_);

  // The pipeline prerolls, and the warm-up starts.
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::DeviceWarmupWaiting(*engine_); }));
  ASSERT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // Buffering started, and lasts longer than the warm-up.
  GstEngineTest::SetBuffering(*engine_, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return !GstEngineTest::DeviceWarmupWaiting(*engine_); }));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::PlayPending(*engine_));

  // Playback starts when buffering finished.
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_FALSE(GstEngineTest::Buffering(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// A state requested after the warm-up elapsed while buffering replaces starting playback when buffering finished.
TEST_F(GstEngineSeekTest, NewerStateReplacesPlaybackAfterBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 100);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::DeviceWarmupWaiting(*engine_); }));
  GstEngineTest::SetBuffering(*engine_, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return !GstEngineTest::DeviceWarmupWaiting(*engine_); }));
  ASSERT_TRUE(GstEngineTest::PlayPending(*engine_));

  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  EXPECT_FALSE(GstEngineTest::PlayPending(*engine_));

  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: when buffering started while playing before a seek reset, the request to play when buffering finished was kept, and started playback before the audio device warm-up elapsed.
TEST_F(GstEngineSeekTest, BufferingFromBeforeSeekResetDoesNotSkipDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);

  // Buffering started while playing, so playback is resumed when buffering finished.
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);
  ASSERT_TRUE(GstEngineTest::Buffering(*engine_));
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  GstEngineTest::PipelineReady(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

}

// A request to play when buffering finished doesn't start playback while waiting for the audio device warm-up, the warm-up starts it.
TEST_F(GstEngineSeekTest, BufferingFinishedDuringDeviceWarmupDoesNotStartPlayback) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::DeviceWarmupWaiting(*engine_); }));

  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::SetPendingState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::BufferingFinished(*engine_);

  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // Playback starts when the warm-up elapsed.
  // The track is short, so when it plays, it can play to the end, and the pipeline is removed.
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: when buffering paused playback before a seek, the pause for buffering was taken as a pause, so the next track stayed paused after the seek reset.
TEST_F(GstEngineSeekTest, PausedForBufferingResumesAfterSeekReset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);

  // Buffering started while playing, which pauses playback until buffering finished.
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);

  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  ASSERT_TRUE(GstEngineTest::NextUrlNeedsReset(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::SeekResetPreviousState(*engine_));

  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);

  // Paused until buffering finished, then playing.
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// A track which was paused, not for buffering, stays paused after the seek reset, also when buffering.
TEST_F(GstEngineSeekTest, PausedWhileBufferingIsRestoredAsPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SetBuffering(*engine_, true);
  ASSERT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  ASSERT_TRUE(GstEngineTest::NextUrlNeedsReset(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::SeekResetPreviousState(*engine_));
  GstEngineTest::SetBuffering(*engine_, false);

}

// Regression test: pausing while the pipeline went to READY for a seek reset did nothing, since playback was not pending, and the seek reset resumed playback.
TEST_F(GstEngineSeekTest, PauseDuringSeekResetIsHeld) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::SeekResetPreviousState(*engine_));

  // The pipeline is in PAUSED on the way to READY.
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);
  engine_->Pause();
  EXPECT_EQ(EngineBase::State::Paused, engine_->state());

  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(EngineBase::State::Paused, engine_->state());

  // Playback is held when the pipeline is prerolled, and unpausing starts it.
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(EngineBase::State::Paused, engine_->state());
  engine_->Unpause();
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: unpausing after the seek reset, before the pipeline was prerolled, started the warm-up in READY, and when prerolling took longer than the warm-up, playback was not started.
TEST_F(GstEngineSeekTest, UnpauseBeforePrerollAfterSeekResetWarmsUpAfterPreroll) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);
  engine_->Pause();
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);

  engine_->Unpause();
  EXPECT_EQ(EngineBase::State::Playing, engine_->state());
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

  // The warm-up starts when the pipeline is prerolled.
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: the request to play after the audio device warm-up during the reset of the next uri was taken when going down to PAUSED on the way to READY, so the warm-up could elapse before the pipeline prerolled again, and playback was not started.
TEST_F(GstEngineSeekTest, DeviceWarmupIsNotStartedOnTheWayToReady) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PAUSED);
  GstEngineTest::RequestState(*engine_, GST_STATE_PLAYING);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

  // Going down to PAUSED on the way to READY.
  GstEngineTest::PipelinePausedOnTheWayToReady(*engine_);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));

  // The next uri is reverted, and the pipeline prerolls again, the seek is applied, and then starts the warm-up.
  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupPending(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

}

// Regression test: a seek for the next track after the next stream started, but before the pipeline went to READY for the reset, was applied right away, and lost when the pipeline went to READY.
TEST_F(GstEngineSeekTest, SeekAfterNextStreamStartedIsAppliedAfterReset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kThirdSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kThirdSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));
  EXPECT_EQ(kThirdSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));

  // The pipeline prerolls after READY, also without the warm-up, and the seek is applied before playback starts.
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));
  // The queued request to preroll is applied, then the pipeline is prerolled.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // Only the queued calls are processed, not the messages of the pipeline, the seek is applied, then the request to play after it.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: when the pipeline prerolled for Play() with the warm-up while buffering, with a pending seek, the warm-up started, and could elapse before buffering finished and the seek was applied, so playback started from the old position.
TEST_F(GstEngineSeekTest, PrerolledWhileBufferingKeepsPlaybackForSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  GstEngineTest::SetPendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::SetBuffering(*engine_, true);

  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  // The seek is taken when buffering finished.
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

}

// Regression test: unpausing while the seek after the next uri was reset waited for the seek delay started playback before the seek.
TEST_F(GstEngineSeekTest, UnpauseDuringDelayedResetSeekPlaysAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::QueueTakenSeekDelayed(*engine_, GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec));
  GstEngineTest::RequestState(*engine_, GST_STATE_PLAYING);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  // The seek is applied after the seek delay, and starts the warm-up, the track is short, so when it plays, it can play to the end, and the pipeline is removed.
  ASSERT_TRUE(WaitFor([this]() { return !GstEngineTest::HasPipeline(*engine_) || GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: a pause while the next uri was reverted for the seek was replaced by the state from before the seek, so playback was resumed after the seek.
TEST_F(GstEngineSeekTest, PauseDuringNextUriRevertStaysPausedAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::PipelineReady(*engine_);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

}

// Without a newer request, the state from before the seek is restored after the next uri was reverted.
TEST_F(GstEngineSeekTest, PlayingBeforeNextUriRevertResumesAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::PipelineReady(*engine_);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

}

// Regression test: a pipeline created while another pipeline was only loaded skipped the warm-up, but the loaded pipeline did not open the audio device.
TEST_F(GstEngineSeekTest, PipelineAfterLoadedPipelineUsesDeviceWarmup) {

  ASSERT_TRUE(Load());
  ASSERT_TRUE(Load());
  EXPECT_EQ(500, GstEngineTest::DeviceWarmupDuration(*engine_));

}

// Regression test: a pipeline created while another pipeline was only prerolled, like when paused, skipped the warm-up, but the audio device has been idle, and the output can be suspended.
TEST_F(GstEngineSeekTest, PipelineAfterPrerolledPipelineUsesDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && !GstEngineTest::Buffering(*engine_); }));
  ASSERT_TRUE(Load());
  EXPECT_EQ(500, GstEngineTest::DeviceWarmupDuration(*engine_));

}

// A pipeline created while another pipeline is playing skips the warm-up, the audio device is already active.
TEST_F(GstEngineSeekTest, PipelineAfterPlayingPipelineSkipsDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelinePlaying(*engine_);
  ASSERT_TRUE(Load());
  EXPECT_EQ(0, GstEngineTest::DeviceWarmupDuration(*engine_));

}

// Regression test: unpausing while the next uri was reset requested PLAYING, and skipped the warm-up, but the reset closed the audio device, so the start of the track was cut off.
TEST_F(GstEngineSeekTest, UnpauseDuringSeekResetWaitsForDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PAUSED);
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::RequestState(*engine_, GST_STATE_PLAYING);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  EXPECT_TRUE(GstEngineTest::PlayPending(*engine_));

  // The newer request replaces restoring the state from before the seek, the warm-up starts when the pipeline is prerolled.
  GstEngineTest::PipelineReady(*engine_);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupPending(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: Play() during a seek reset on a pipeline created while another pipeline was active skipped the warm-up, but the reset closed the audio device, so the start of the track was cut off.
TEST_F(GstEngineSeekTest, PlayDuringSeekResetUsesSkippedDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmupSkipped(*engine_, 500);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PAUSED);
  // The reset closes the audio device, so the warm-up for opening it again is used.
  EXPECT_EQ(500, GstEngineTest::DeviceWarmupDuration(*engine_));
  GstEngineTest::Play(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

  // The pipeline prerolls again, and the seek starts the warm-up before playing.
  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::PipelinePaused(*engine_);
  ProcessEventsForSeekDelay();
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// A request to play while the next uri is reset without the warm-up is applied.
// Regression test: the request to play was replaced by prerolling paused after the next uri was reverted, so the pipeline stayed paused after the seek.
TEST_F(GstEngineSeekTest, UnpauseDuringSeekResetWithoutDeviceWarmupPlays) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  SharedPtr<GstEngineTest::SegmentObservation> segments = make_shared<GstEngineTest::SegmentObservation>(kFirstSeekNanosec);
  GstEngineTest::ObserveSegments(*engine_, segments);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PAUSED);
  GstEngineTest::ClearNextUrl(*engine_);
  // The pipeline doesn't go to PLAYING before the uri is reverted and the seek is applied.
  GstEngineTest::RequestState(*engine_, GST_STATE_PLAYING);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

  // The next uri is reverted, and the pipeline prerolls paused, the request to play is kept for the seek.
  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  // The seek is applied after the seek delay, and starts playback, the segment of the seek is recorded also when the track ended right after it.
  EXPECT_TRUE(WaitFor([&segments]() { return segments->sink_state.load() != GstEngineTest::SegmentSinkState::NotReached; }));
  EXPECT_EQ(GstEngineTest::SegmentSinkState::NotPlaying, segments->sink_state.load());
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: when the pipeline prerolled for Play() with the warm-up, a seek which was taken by the pad callback, but not applied yet, was not waited for, so the warm-up started, and playback could start from the old position.
TEST_F(GstEngineSeekTest, DeviceWarmupWaitsForTakenSeekWhenPrerolled) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

  // The pad callback took the seek, and the pipeline prerolled before it is queued.
  const GstEngineTest::TakenSeek taken_seek = GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  // The seek starts the warm-up.
  GstEngineTest::QueueTakenSeek(*engine_, taken_seek);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: when buffering finished before the pause for buffering was applied, and left the request to play to a pending seek, the pause replaced the request, so the seek did not start playback.
TEST_F(GstEngineSeekTest, PauseForBufferingKeepsPlaybackAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // Buffering starts while playing, and the pause for it is queued, buffering finishes with a pending seek before the pause is applied.
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::SetTargetState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::BufferingMessage(*engine_, 50);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  GstEngineTest::SetPendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // Only the queued calls are processed, not the messages of the pipeline, the pause is applied, then the seek starts the warm-up.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// An explicit pause still replaces the request to play after a seek.
TEST_F(GstEngineSeekTest, PauseReplacesPlaybackAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetPendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

}

// Regression test: the warm-up was skipped after a seek reset on a pipeline created while another pipeline was active, the reset closed the audio device, so the start of the track was cut off.
TEST_F(GstEngineSeekTest, SkippedDeviceWarmupIsUsedAfterSeekReset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmupSkipped(*engine_, 500);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: when buffering started before the request to play after a seek reset was applied, playback was started while buffering.
TEST_F(GstEngineSeekTest, BufferingBeforeQueuedPlayingAfterSeekResetDefersPlayback) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

  GstEngineTest::BufferingMessage(*engine_, 50);
  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: when buffering finished after the next uri was reset, but before the pipeline was paused again, Play() from an offset was replaced by going to PAUSED for the reset.
TEST_F(GstEngineSeekTest, BufferingFinishedAfterNextUriResetKeepsPlayFromOffset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PAUSED);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  // The next uri is reset, and the pipeline goes to PAUSED, buffering finishes before that.
  GstEngineTest::PipelineReady(*engine_);
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: when a pad was added before READY was handled for the seek reset, the seek was taken, so the reset found no seek, and the pipeline stayed in READY.
TEST_F(GstEngineSeekTest, SeekWaitingForNextUriResetIsNotTakenWhenPadAdded) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::QueuePendingSeek(*engine_);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  // The next uri is reset, the seek is still not taken until the pipeline is paused again.
  GstEngineTest::PipelineReady(*engine_);
  GstEngineTest::QueuePendingSeek(*engine_);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: a seek while waiting for the audio device warm-up when the next uri was set saved PAUSED, the reset cancelled the warm-up, so playback was not resumed.
TEST_F(GstEngineSeekTest, DeviceWarmupWaitingBeforeSeekResetIsResumed) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SetDeviceWarmupWaiting(*engine_);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::SeekResetPreviousState(*engine_));

}

// Regression test: a seek when the audio device warm-up elapsed while buffering and the next uri was set saved PAUSED, the reset cancelled starting playback, so playback was not resumed.
TEST_F(GstEngineSeekTest, DeviceWarmupElapsedWhileBufferingBeforeSeekResetIsResumed) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SetDeviceWarmupElapsedWhileBuffering(*engine_);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::SeekResetPreviousState(*engine_));

}

// Regression test: a seek when the request to play after buffering was queued, but not applied yet, and the next uri was set saved PAUSED, the reset replaced the request, so the next track stayed paused.
TEST_F(GstEngineSeekTest, QueuedPlayingBeforeSeekResetIsResumed) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::SetPendingState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));
  ASSERT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::SeekResetPreviousState(*engine_));

}

// Regression test: a seek when playback was requested, and the state was still READY, like before the state message after prerolling was handled, kept READY, so the reset did not resume playback.
TEST_F(GstEngineSeekTest, ReadyWithPlayingRequestedBeforeSeekResetIsResumed) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_READY);
  GstEngineTest::RequestState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::SeekResetPreviousState(*engine_));

}

// Regression test: a seek reset while the pipeline was still READY, and prerolling paused, restored READY, so the pipeline stayed in READY after the next stream started, and the pause was lost.
TEST_F(GstEngineSeekTest, ReadyWithPauseRequestedBeforeSeekResetIsPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_READY);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::SeekResetPreviousState(*engine_));

  // The next stream started before the reset reached READY, the pipeline prerolls paused, and is still paused.
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(EngineBase::State::Paused, engine_->state());

}

// Without a request to play, READY is kept after the seek reset.
TEST_F(GstEngineSeekTest, ReadyBeforeSeekResetStaysReady) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_READY);
  GstEngineTest::RequestState(*engine_, GST_STATE_READY);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_READY, GstEngineTest::SeekResetPreviousState(*engine_));

}

// When buffering finished after the next uri was reverted, a pause before the pipeline prerolled holds playback, the request to resume from the buffering doesn't start it after the seek.
TEST_F(GstEngineSeekTest, PauseAfterBufferingFinishedDuringNextUriRevertHoldsPlayback) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);
  ASSERT_TRUE(GstEngineTest::Buffering(*engine_));
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_FALSE(GstEngineTest::Buffering(*engine_));
  engine_->Pause();

  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_TRUE(WaitForPipeline([this]() { return GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_NE(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: a state requested while the pipeline went to READY for the reset of the next uri, like a request to play, which prerolls paused, or a pause, was applied before READY was handled, so the pipeline prerolled the next uri, and the uri was reverted while the state change was in progress.
TEST_F(GstEngineSeekTest, RequestBeforeNextUriRevertIsAppliedAfterReady) {

  ASSERT_TRUE(Load());
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  EXPECT_EQ(GST_STATE_READY, GstEngineTest::AppliedState(*engine_));

  // The READY message reverts the uri, and the pipeline prerolls, applies the seek, and stays paused, the track is short, so the pipeline is removed if it plays.
  EXPECT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::PipelineState(*engine_));

}

// Regression test: a pause after unpausing while the next uri was reset kept the request to play after the warm-up, so playback started after the pipeline prerolled again.
TEST_F(GstEngineSeekTest, PauseAfterUnpauseDuringSeekResetStaysPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PAUSED);
  GstEngineTest::RequestState(*engine_, GST_STATE_PLAYING);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupPending(*engine_));

  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: when buffering started and finished while the next uri was reset, a pause before READY was handled was replaced by the request to resume from the buffering, so the seek started playback.
TEST_F(GstEngineSeekTest, PauseAfterBufferingFinishedBeforeNextUriRevertStaysPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_FALSE(GstEngineTest::Buffering(*engine_));
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  // The pipeline is still playing, so this pauses it.
  engine_->Pause();
  ASSERT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));
  EXPECT_TRUE(WaitForPipeline([this]() { return GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_NE(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// A request to play after buffering started and finished while the next uri was reset is kept, it is not the request to resume from the buffering, which a newer request replaces.
TEST_F(GstEngineSeekTest, PlayAfterBufferingFinishedBeforeNextUriRevertPlaysAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_FALSE(GstEngineTest::Buffering(*engine_));
  GstEngineTest::RequestState(*engine_, GST_STATE_PLAYING);

  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

}

// Regression test: a pause for buffering cancelled the warm-up which a seek started, so playback was not started when buffering finished.
TEST_F(GstEngineSeekTest, BufferingPauseKeepsDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetDeviceWarmup(*engine_, 100);
  GstEngineTest::StartPlaybackAfterWarmup(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  GstEngineTest::BufferingPause(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

  // The warm-up still starts playback, when it elapses while buffering, playback is started when buffering finished.
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: a pause during the seek reset while the pipeline was still playing kept playing as the state to restore, so playback still counted as pending, and the pipeline was reported as playing.
TEST_F(GstEngineSeekTest, PauseDuringSeekResetIsNotPlayPending) {

  ASSERT_TRUE(Load());
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::SetPipelinePlaying(*engine_);
  engine_->Pause();
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::SeekResetPreviousState(*engine_));
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);
  EXPECT_FALSE(GstEngineTest::PlayPending(*engine_));

}

// Regression test: when the pad callback took the seek before the message for prerolling was handled, the request to play was applied by that message, so playback started before the seek.
TEST_F(GstEngineSeekTest, PrerolledWithTakenSeekPlaysAfterSeek) {

  ASSERT_TRUE(Load());
  const GstEngineTest::TakenSeek taken_seek = GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::SetPendingState(*engine_, GST_STATE_PLAYING);

  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_NE(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // The seek starts playback after the warm-up.
  GstEngineTest::QueueTakenSeek(*engine_, taken_seek);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: when buffering started during the seek reset, and finished after the next uri was reverted, a pause before READY was handled was replaced by the request to resume from the buffering, so the seek started playback.
TEST_F(GstEngineSeekTest, PauseBeforeBufferingFinishedAfterNextUriRevertStaysPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);

  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_FALSE(GstEngineTest::Buffering(*engine_));
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));
  EXPECT_TRUE(WaitForPipeline([this]() { return GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_NE(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// When buffering finished after the next uri was reverted, the state from before the seek is restored, and the request is not from buffering any more when the reset finished, otherwise a later request replaces it as a request from buffering.
TEST_F(GstEngineSeekTest, BufferingFinishedAfterNextUriRevertResumesAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);

  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_TRUE(GstEngineTest::PendingStateFromBuffering(*engine_));
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  EXPECT_FALSE(GstEngineTest::PendingStateFromBuffering(*engine_));

}

// Regression test: when the section changed before the seek after the next uri was reverted was applied, the seek was skipped, and the restored request to play was not applied, so the pipeline stayed paused.
TEST_F(GstEngineSeekTest, SkippedSeekAfterNextUriRevertResumesPlayback) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // The state messages are not faked, the message for prerolling again after the revert is the only one which can start playback.
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::QueuedSeeks(*engine_) > 0; }));
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  // The seek is skipped, and playback is started, the track is short, so it can play to the end, and the pipeline is removed.
  GstEngineTest::ChangeTrack(*engine_);
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: a request to play while the seek to the offset of Play() on a prerolled pipeline was queued started playback before the seek.
TEST_F(GstEngineSeekTest, PlayWhileSeekIsQueuedPlaysAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec, true);
  GstEngineTest::RequestState(*engine_, GST_STATE_PLAYING);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  // The seek is applied, and starts playback.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: when the next stream started before the seek reset, a seek for the next track which was taken, but not applied yet, like by the pad callback right after the flag for the reset was cleared, was not waited for without the warm-up, so playback started before the seek.
TEST_F(GstEngineSeekTest, TakenSeekAfterNextStreamStartedBeforeSeekResetPlaysAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);
  // The seek is taken in the READY handler, after the flag for the reset was cleared, and before the reasons to preroll paused are checked, which the test can't interleave, so it is taken before the handler, the state is the same.
  const GstEngineTest::TakenSeek taken_seek = GstEngineTest::TakePendingSeek(*engine_, kThirdSeekNanosec);
  ASSERT_EQ(kThirdSeekNanosec, taken_seek.nanosec);
  ASSERT_EQ(-1, GstEngineTest::PendingSeek(*engine_));

  // The pipeline prerolls paused, and doesn't play before the taken seek is applied.
  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::DeviceWarmupAfterSeek(*engine_); }));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  // The taken seek is applied, and then starts playback, the segment of the seek is recorded also when the track ended right after it.
  SharedPtr<GstEngineTest::SegmentObservation> segments = make_shared<GstEngineTest::SegmentObservation>(kThirdSeekNanosec);
  GstEngineTest::ObserveSegments(*engine_, segments);
  GstEngineTest::QueueTakenSeek(*engine_, taken_seek);
  EXPECT_TRUE(WaitFor([&segments]() { return segments->sink_state.load() != GstEngineTest::SegmentSinkState::NotReached; }));
  EXPECT_EQ(GstEngineTest::SegmentSinkState::NotPlaying, segments->sink_state.load());
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: when a pause was requested right before a seek reset, the state was still PLAYING, so buffering requested to resume playback, and that request resumed playback over the pause after the next stream started.
TEST_F(GstEngineSeekTest, BufferingAfterPauseBeforeSeekResetStaysPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PAUSED);

  GstEngineTest::BufferingMessage(*engine_, 50);
  EXPECT_NE(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  GstEngineTest::BufferingFinished(*engine_);

  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  GstEngineTest::PipelinePaused(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

}

// Regression test: a pause after Play() from an offset while the next uri was reset did not replace the request to play of Play(), so the seek to the offset started playback over the pause after the next stream started.
TEST_F(GstEngineSeekTest, PauseAfterPlayFromOffsetDuringSeekResetStaysPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  // The state is still PLAYING until the pipeline went to READY, so the pipeline is paused.
  GstEngineTest::SetPipelinePlaying(*engine_);
  engine_->Pause();
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

  // The offset is still applied, and the pipeline stays paused.
  GstEngineTest::PipelineReady(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));
  EXPECT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: a pause before the pipeline left PLAYING for a seek reset was reported as idle after READY, until the pipeline prerolled again, so resuming in between was ignored.
TEST_F(GstEngineSeekTest, UnpauseAfterPauseDuringSeekResetBeforePrerollPlays) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);
  // The state is still PLAYING until the pipeline went to READY, so the pipeline is paused.
  GstEngineTest::SetPipelinePlaying(*engine_);
  engine_->Pause();
  EXPECT_EQ(EngineBase::State::Paused, engine_->state());

  // The next stream started, and the pipeline went to READY, it is still paused.
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(EngineBase::State::Paused, engine_->state());

  // Resuming before the pipeline prerolled again starts playback after the preroll.
  engine_->Unpause();
  EXPECT_EQ(EngineBase::State::Playing, engine_->state());
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: a request to play paused while the next uri was reset finished right away, before an older request to play whose state change was still running, so the older request reported playing after the pause.
TEST_F(GstEngineSeekTest, OlderPlayIsNotReportedAfterDeferredPlayPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  QList<EngineBase::State> states;
  // Disconnects the callbacks before the variables they use are destroyed, also when the test fails early, and the fixture still processes events after that.
  QObject callbacks_context;
  QObject::connect(&*engine_, &EngineBase::StateChanged, &callbacks_context, [&states](const EngineBase::State state) { states << state; });
  // The state change of the older request to play is still running.
  SharedPtr<QSemaphore> state_changes_blocked = make_shared<QSemaphore>();
  const auto release_state_changes = qScopeGuard([state_changes_blocked]() { state_changes_blocked->release(); });
  GstEngineTest::BlockStateChanges(*engine_, state_changes_blocked);
  ASSERT_TRUE(engine_->Play(false, 0));
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);

  // The newer request is deferred until READY, and finishes first.
  ASSERT_TRUE(engine_->Play(true, 0));
  EXPECT_TRUE(WaitFor([&states]() { return !states.isEmpty(); }));
  ASSERT_EQ(QList<EngineBase::State>{EngineBase::State::Paused}, states);

  // The older request finishes, it is not reported.
  state_changes_blocked->release();
  ProcessEventsFor(200);
  EXPECT_FALSE(states.contains(EngineBase::State::Playing));

}

// Regression test: a preroll which was queued before the pipeline went to READY for a seek reset ended the reset, so after the reset went to READY, the pause was reported as idle, and unpausing was ignored.
TEST_F(GstEngineSeekTest, PrerollQueuedBeforeSeekResetKeepsPause) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_READY);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  GstEngineTest::ClearNextUrl(*engine_);

  // The preroll from before the reset, then the next stream started, and the pipeline went to READY.
  GstEngineTest::PipelinePaused(*engine_);
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(EngineBase::State::Paused, engine_->state());

  engine_->Unpause();
  EXPECT_EQ(EngineBase::State::Playing, engine_->state());

}

// Regression test: when buffering finished after the next stream started, the request to play of Play() from an offset during the reset was handed to the seek, then the restored pause, which was queued before, replaced it, so the pipeline stayed paused after the seek.
TEST_F(GstEngineSeekTest, BufferingFinishedAfterNextStreamStartedKeepsPlayFromOffset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::PlayFromOffset(*engine_, kThirdSeekNanosec);
  GstEngineTest::PipelineReady(*engine_);

  // The source of the next track buffers, and finishes before the queued state request is applied.
  GstEngineTest::BufferingMessage(*engine_, 50);
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: an unpause right after a request to play paused, which was deferred while the next uri was reset, was reported first, then the completion of the request to play reported paused, although playback resumes.
TEST_F(GstEngineSeekTest, UnpauseAfterDeferredPlayPausedIsReported) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  QList<EngineBase::State> states;
  bool valid_song_requested = false;
  // Disconnects the callbacks before the variables they use are destroyed, also when the test fails early, and the fixture still processes events after that.
  QObject callbacks_context;
  QObject::connect(&*engine_, &EngineBase::StateChanged, &callbacks_context, [&states](const EngineBase::State state) { states << state; });
  QObject::connect(&*engine_, &EngineBase::ValidSongRequested, &callbacks_context, [&valid_song_requested](const QUrl &url) {
    Q_UNUSED(url)
    valid_song_requested = true;
  });
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);

  // The request to play paused is deferred until READY, and unpaused before its completion is reported.
  ASSERT_TRUE(engine_->Play(true, 0));
  ASSERT_EQ(EngineBase::State::Paused, engine_->state());
  engine_->Unpause();
  ASSERT_EQ(QList<EngineBase::State>{EngineBase::State::Playing}, states);

  // The completion still reports the started song, but not the older paused state.
  EXPECT_TRUE(WaitFor([&valid_song_requested]() { return valid_song_requested; }));
  EXPECT_EQ(QList<EngineBase::State>{EngineBase::State::Playing}, states);

}

// Regression test: when the next stream started while the pipeline was reset for a seek, and the state was still PLAYING, the start offset of the next track, like for a CUE section, waited for the seek delay of the engine, so without the warm-up, the pipeline resumed playback before the offset.
TEST_F(GstEngineSeekTest, PlayWithOffsetAfterNextStreamStartedDuringSeekResetPlaysFromOffset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);
  GstEngineTest::StartNextStream(*engine_);
  // The state is still PLAYING, and the seek which started the reset was just made, so a seek of the engine waits for the seek delay.
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::StartSeekTimer(*engine_);

  // The player starts the next track from the start of its CUE section.
  SharedPtr<GstEngineTest::SegmentObservation> segments = make_shared<GstEngineTest::SegmentObservation>(kThirdSeekNanosec);
  GstEngineTest::ObserveSegments(*engine_, segments);
  GstEngineTest::SetBeginningOffset(*engine_, static_cast<quint64>(kThirdSeekNanosec));
  ASSERT_TRUE(engine_->Play(false, 0));
  EXPECT_EQ(kThirdSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  // The pipeline prerolls paused after READY, and plays from the offset.
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(WaitFor([&segments]() { return segments->sink_state.load() != GstEngineTest::SegmentSinkState::NotReached; }));
  EXPECT_EQ(GstEngineTest::SegmentSinkState::NotPlaying, segments->sink_state.load());
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: a request to start paused, when the next stream started while the pipeline was reset for a seek, and the state was still PLAYING, was ignored like for a pipeline which is already playing, so the state from before the seek was restored over it, and playback resumed.
TEST_F(GstEngineSeekTest, PlayPausedAfterNextStreamStartedDuringSeekResetStaysPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  QList<EngineBase::State> states;
  // Disconnects the callback before the variable it uses is destroyed, also when the test fails early, and the fixture still processes events after that.
  QObject callbacks_context;
  QObject::connect(&*engine_, &EngineBase::StateChanged, &callbacks_context, [&states](const EngineBase::State state) { states << state; });
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);
  GstEngineTest::StartNextStream(*engine_);
  // The state is still PLAYING from before the reset.
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);

  // The player starts the next track paused, the pipeline prerolls paused after READY, instead of restoring playback.
  ASSERT_TRUE(engine_->Play(true, 0));
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(WaitFor([&states]() { return states.contains(EngineBase::State::Paused); }));
  // The pipeline is removed when the track played to the end.
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_FALSE(states.contains(EngineBase::State::Playing));

}

// Regression test: a seek which waited for the seek delay of the engine replaced the start offset of a newer request to play for the next track during a seek reset, like for a CUE section, when the delay elapsed.
TEST_F(GstEngineSeekTest, OlderEngineSeekDoesNotReplaceOffsetOfPlayDuringSeekReset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::ClearNextUrl(*engine_);
  GstEngineTest::StartNextStream(*engine_);
  // The state is still PLAYING from before the reset.
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);

  // A seek in the next track waits for the seek delay of the engine, then the player starts the next track from the start of its CUE section.
  GstEngineTest::StartSeekTimer(*engine_);
  engine_->Seek(static_cast<quint64>(kSecondSeekNanosec));
  GstEngineTest::SetBeginningOffset(*engine_, static_cast<quint64>(kThirdSeekNanosec));
  ASSERT_TRUE(engine_->Play(false, 0));
  ASSERT_EQ(kThirdSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  // The delay elapses before the pipeline went to READY, the older seek doesn't replace the offset.
  GstEngineTest::SeekDelayElapsed(*engine_);
  EXPECT_EQ(kThirdSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: when the pipeline prerolled, and queued a seek which replaced the offset of Play(), and the section changed before the seek was applied, the seek was skipped without starting playback, so the pipeline stayed paused.
TEST_F(GstEngineSeekTest, SeekQueuedByPrerollSkippedAfterSectionChangeStartsPlayback) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  // A seek in the track replaces the offset before the pipeline prerolled.
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  ASSERT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

  // The pipeline prerolled and queued the seek, then the section changed before it was applied.
  GstEngineTest::PipelinePaused(*engine_);
  ASSERT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  GstEngineTest::ChangeTrack(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// A pause which is applied is kept after the seek reset.
TEST_F(GstEngineSeekTest, PausedBeforeSeekResetStaysPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::SeekResetPreviousState(*engine_));

}

// Regression test: when buffering finished before the seek reset, the request to resume playback waited for the reset, and a newer pause superseding the reset did not replace it, so playback was resumed over the pause.
TEST_F(GstEngineSeekTest, NewerPauseAfterBufferingStaysPausedAfterSeekReset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);
  // Buffering starts while the pipeline is still playing, and finishes before the reset, the request to resume playback waits for the reset.
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::BufferingMessage(*engine_, 50);
  GstEngineTest::BufferingFinished(*engine_);
  ASSERT_FALSE(GstEngineTest::Buffering(*engine_));
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

}

// Regression test: when Play() from an offset queued the seek when the pipeline prerolled, and buffering finished before the seek was applied, playback was started before the seek and the warm-up.
TEST_F(GstEngineSeekTest, BufferingFinishedBeforeQueuedOffsetSeekKeepsPlaybackForSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  ASSERT_FALSE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // The pipeline prerolled, and the seek to the offset is taken and queued, buffering starts and finishes before it is applied.
  GstEngineTest::QueueTakenSeek(*engine_, GstEngineTest::TakePendingSeek(*engine_, kSecondSeekNanosec));
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

}

// Regression test: a pause which was requested, but not applied yet when seeking while the next uri was set, was not kept, the state was still PLAYING, so playback was resumed after the reset.
TEST_F(GstEngineSeekTest, PauseBeforeSeekResetIsKept) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::SeekResetPreviousState(*engine_));

}

// Playback requested by Play() from an offset, which is not applied yet when seeking while the next uri was set, is resumed after the reset.
TEST_F(GstEngineSeekTest, PlayFromOffsetBeforeSeekResetIsResumed) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::SeekResetPreviousState(*engine_));

}

// Playback requested by Play() waiting for the audio device warm-up, which is not applied yet when seeking while the next uri was set, is resumed after the reset.
TEST_F(GstEngineSeekTest, PlayWithDeviceWarmupBeforeSeekResetIsResumed) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));
  GstEngineTest::SetNextUrl(*engine_, url_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kFirstSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::SeekResetPreviousState(*engine_));

}

// Regression test: when a newer pause superseded the seek reset while buffering, the source for the next track ended the buffering of the previous source and started playback over the pause.
TEST_F(GstEngineSeekTest, NewerPauseWhileBufferingStaysPausedAfterSeekReset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);
  // Buffering starts while the pipeline is still playing, so it requests to resume playback when buffering finished.
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::BufferingMessage(*engine_, 50);
  ASSERT_TRUE(GstEngineTest::Buffering(*engine_));
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);

  GstEngineTest::PipelineReady(*engine_);
  EXPECT_FALSE(GstEngineTest::Buffering(*engine_));
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

  // The pipeline prerolls with a new source, and stays paused, the track is short, so when it plays, it plays to the end and the pipeline is removed.
  ASSERT_TRUE(WaitFor([this]() { return !GstEngineTest::HasPipeline(*engine_) || GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED || GstEngineTest::TargetState(*engine_) == GST_STATE_PLAYING; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: when buffering started and finished after prerolling for the warm-up, before the queued seek was applied, buffering finished started playback, and the seek didn't start the warm-up.
TEST_F(GstEngineSeekTest, BufferingBeforeQueuedSeekDoesNotSkipDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, GstEngineTest::TrackChanges(*engine_)));

  // Prerolled, the seek is queued.
  GstEngineTest::PipelinePaused(*engine_);
  ASSERT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  ASSERT_TRUE(GstEngineTest::PlayPending(*engine_));

  // Buffering starts and finishes before the queued seek is applied.
  GstEngineTest::BufferingMessage(*engine_, 50);
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // The queued seek is applied, and starts the warm-up, only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_FALSE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: when the track changed after prerolling for the warm-up, before the queued seek was applied, the seek was not applied, and playback was not started.
TEST_F(GstEngineSeekTest, TrackChangeBeforeQueuedSeekStartsPlaybackAfterWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  GstEngineTest::PipelinePaused(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  GstEngineTest::ChangeTrack(*engine_);
  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_NE(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

}

// When the queued seek is ignored, since the pipeline is already at the start of the next section, playback is still started after the warm-up.
TEST_F(GstEngineSeekTest, IgnoredQueuedSeekStartsPlaybackAfterWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  GstEngineTest::PipelinePaused(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  GstEngineTest::SetIgnoreNextSeek(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_NE(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// A seek for the previous track doesn't start playback which is waiting for the seek to a start offset.
TEST_F(GstEngineSeekTest, SeekForPreviousTrackKeepsPlaybackFromOffsetPending) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  // The pipeline is not prerolled yet, so the offset and playback are pending.
  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  const int track_changes = GstEngineTest::TrackChanges(*engine_);
  GstEngineTest::ChangeTrack(*engine_);

  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, track_changes));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// A newer pause before the queued seek for the previous track keeps the pipeline paused.
TEST_F(GstEngineSeekTest, NewerPauseBeforeQueuedSeekForPreviousTrackStaysPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  GstEngineTest::PipelinePaused(*engine_);

  GstEngineTest::ChangeTrack(*engine_);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// A newer state request replaces starting playback from the queued seek after the warm-up.
TEST_F(GstEngineSeekTest, NewerStateReplacesPlaybackAfterQueuedSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  GstEngineTest::PipelinePaused(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

  // The queued seek is applied, and doesn't start playback, only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Starting playback from an offset while an older queued seek is going to start playback keeps the request to play from the offset.
TEST_F(GstEngineSeekTest, PlayFromOffsetReplacesPlaybackAfterQueuedSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  // The pipeline is prerolled first, otherwise GStreamer doesn't apply the seeks.
  GstEngineTest::PlayFromOffset(*engine_, kThirdSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kThirdSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // The older seek is queued, and it starts playback.
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // The seek to the offset is queued too, and it starts playback.
  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  // The older queued seek is not applied, the seek to the offset is, and it starts playback after the warm-up, only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

  // GStreamer applied the seek to the offset, not the older seek, and the pipeline didn't stay at the position before, so playback starts there.
  EXPECT_TRUE(WaitFor([this]() {
    if (!GstEngineTest::HasPipeline(*engine_)) return false;
    const qint64 position = GstEngineTest::QueriedPosition(*engine_);
    return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && position >= kFirstSeekNanosec && position < kThirdSeekNanosec;
  }));

}

// Regression test: starting paused from an offset while an older queued seek was going to start playback kept its request to play, so playback started after the warm-up.
TEST_F(GstEngineSeekTest, PlayPausedFromOffsetReplacesPlaybackAfterQueuedSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kSecondSeekNanosec, GstEngineTest::TrackChanges(*engine_)));
  GstEngineTest::PipelinePaused(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec, true);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));

  // The offset is applied, and playback stays paused.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  ASSERT_TRUE(WaitFor([this]() { return GstEngineTest::HasPipeline(*engine_) && GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  ProcessEventsFor(700);
  // The track is short, so when it plays, it plays to the end and the pipeline is removed.
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: starting playback from an offset when the pipeline was already prerolled didn't change the state, so the offset was not applied, and a seek queued after prerolling for the warm-up was dropped, so playback was not started.
TEST_F(GstEngineSeekTest, PlayFromOffsetWhenPrerolledReplacesQueuedSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);

  // The pipeline really prerolls.
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // A seek is queued after prerolling for the warm-up, only the queued calls are processed below, not the messages of the pipeline.
  GstEngineTest::SetSeekAfterDeviceWarmup(*engine_, kSecondSeekNanosec);
  GstEngineTest::PipelinePaused(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(-1, GstEngineTest::PendingSeek(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

  // Playback starts after the warm-up, the track is short, so when it plays, it plays to the end and the pipeline is removed.
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Starting paused from an offset when the pipeline is already prerolled applies the offset, and playback stays paused.
TEST_F(GstEngineSeekTest, PlayPausedFromOffsetWhenPrerolled) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::PipelineState(*engine_));

}

// Starting playback from an offset while the prerolled pipeline is buffering applies the offset and starts playback when buffering finished.
TEST_F(GstEngineSeekTest, PlayFromOffsetWhenPrerolledWaitsForBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  GstEngineTest::BufferingFinished(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: starting paused from an offset on a prerolled pipeline, before the seek to an older offset was applied, kept the older request to play, so playback started.
TEST_F(GstEngineSeekTest, PlayPausedFromOffsetReplacesOlderOffset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // Both are requested before the queued seeks are applied, only the queued calls are processed, not the messages of the pipeline.
  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  GstEngineTest::PlayFromOffset(*engine_, kThirdSeekNanosec, true);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kThirdSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: starting paused from an offset while an older Play() was waiting to preroll for the warm-up kept the warm-up, so playback started after it.
TEST_F(GstEngineSeekTest, PlayPausedFromOffsetReplacesWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec, true);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupPending(*engine_));

  // The pipeline prerolls, the offset is applied, and playback stays paused.
  ASSERT_TRUE(WaitFor([this]() { return GstEngineTest::HasPipeline(*engine_) && GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  ProcessEventsFor(700);
  // The track is short, so when it plays, it plays to the end and the pipeline is removed.
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: without a warm-up, the seek to an offset started playback while buffering, which started after the seek was queued.
TEST_F(GstEngineSeekTest, PlayFromOffsetWithoutWarmupWaitsForBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // The seek is queued, then buffering starts before it is applied, only the queued calls are processed, not the messages of the pipeline.
  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  GstEngineTest::BufferingMessage(*engine_, 50);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_TRUE(GstEngineTest::PlayPending(*engine_));

  // Playback starts when buffering finished.
  GstEngineTest::BufferingFinished(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: when buffering started and finished before the seek to an offset on a prerolled pipeline was applied, buffering finished started playback without the warm-up.
TEST_F(GstEngineSeekTest, PlayFromOffsetWhenPrerolledWarmsUpAfterBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // Buffering starts and finishes before the queued seek is applied, only the queued calls are processed, not the messages of the pipeline.
  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  GstEngineTest::BufferingMessage(*engine_, 50);
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: the seek to ignore after a section change of the same file was still ignored when Play() requested a seek to an offset, so the offset was not applied and playback was not started.
TEST_F(GstEngineSeekTest, PlayFromOffsetAfterSectionChangeIsApplied) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetIgnoreNextSeek(*engine_);
  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: another PAUSED message before the queued seek to an offset was applied started playback without the warm-up.
TEST_F(GstEngineSeekTest, PausedMessageBeforeQueuedSeekKeepsWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  GstEngineTest::PipelinePaused(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: Play() with the warm-up on a prerolled pipeline didn't change the state, so the warm-up was not started, and playback stayed paused.
TEST_F(GstEngineSeekTest, PlayWhenPrerolledStartsWarmupAfterQueuedSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // The seek to the offset is queued, then playback is requested without an offset.
  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  GstEngineTest::Play(*engine_);
  EXPECT_TRUE(GstEngineTest::PlayPending(*engine_));

  // The queued seek is applied first, then the warm-up starts, only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // Playback starts when the warm-up elapsed, the track is short, so when it plays, it can play to the end, and the pipeline is removed.
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// A newer pause replaces starting the warm-up of Play() on a prerolled pipeline.
TEST_F(GstEngineSeekTest, PauseReplacesWarmupWhenPrerolled) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::Play(*engine_);
  GstEngineTest::RequestState(*engine_, GST_STATE_PAUSED);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: the seek taken when the pad of the next pipeline was added was queued after a newer seek to an offset, and was applied after it, so playback started at the older offset.
TEST_F(GstEngineSeekTest, QueuedPendingSeekIsReplacedByNewerOffset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kThirdSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kThirdSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // The streaming thread took the seek, then the newer seek is requested and queued before the streaming thread queued the older seek.
  const GstEngineTest::TakenSeek taken_seek = GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec);
  ASSERT_EQ(kFirstSeekNanosec, taken_seek.nanosec);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  GstEngineTest::QueueTakenSeek(*engine_, taken_seek);
  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// The seek taken when the pad of the next pipeline was added doesn't replace a newer seek to an offset which waits for the pipeline.
TEST_F(GstEngineSeekTest, TakenSeekDoesNotReplaceNewerPendingSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kThirdSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kThirdSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // The newer seek waits for buffering to finish, and the older seek is queued after it was requested.
  const GstEngineTest::TakenSeek taken_seek = GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  GstEngineTest::QueueTakenSeek(*engine_, taken_seek);
  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kThirdSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// Starting paused from an offset replaces the request to resume playback when buffering, which started while playing, finished.
TEST_F(GstEngineSeekTest, PlayPausedFromOffsetReplacesPlaybackAfterBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SetPipelineState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::BufferingMessage(*engine_, 50);
  ASSERT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec, true);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: when the track was paused and buffering before a seek reset, the source for the next track ended buffering and started playback.
TEST_F(GstEngineSeekTest, PausedWhileBufferingStaysPausedAfterSeekReset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PAUSED);
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::SetBuffering(*engine_, true);

  GstEngineTest::PipelineReady(*engine_);
  EXPECT_FALSE(GstEngineTest::Buffering(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // The pipeline prerolls with a new source, and stays paused, the track is short, so when it plays, it plays to the end and the pipeline is removed.
  ASSERT_TRUE(WaitFor([this]() { return !GstEngineTest::HasPipeline(*engine_) || GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED || GstEngineTest::TargetState(*engine_) == GST_STATE_PLAYING; }));
  ProcessEventsFor(200);
  ASSERT_TRUE(GstEngineTest::HasPipeline(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: Play() paused while going to READY for a seek reset while buffering, the source for the newer request was set up before the READY message was handled, and it ended buffering and started playback over the pause.
TEST_F(GstEngineSeekTest, SourceSetupBeforeSeekResetReadyKeepsPlayPaused) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::SetBuffering(*engine_, true);

  GstEngineTest::PlayFromOffset(*engine_, 0, true);
  GstEngineTest::SourceSetup(*engine_);
  EXPECT_FALSE(GstEngineTest::Buffering(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  GstEngineTest::PipelineReady(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: Play() with the warm-up after buffering paused the prerolled pipeline didn't change the state, so the warm-up was not started, and playback stayed paused when buffering finished.
TEST_F(GstEngineSeekTest, PlayWhileBufferingStartsWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 200);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::Play(*engine_);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // Playback starts when buffering finished and the warm-up elapsed, the track is short, so when it plays, it can play to the end, and the pipeline is removed.
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: a message of an older transition to PLAYING handled before the queued seek of Play() from an offset was applied made the seek skip starting playback, and playback stayed paused.
TEST_F(GstEngineSeekTest, PlayingMessageBeforeQueuedSeekKeepsPlayback) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  GstEngineTest::PipelinePlaying(*engine_);
  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: the warm-up elapsed while buffering, then a new source ended buffering without starting playback, and the message for buffering finished was ignored after that, so playback stayed paused.
TEST_F(GstEngineSeekTest, SourceSetupAfterWarmupElapsedWhileBufferingStartsPlayback) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::SetDeviceWarmupElapsedWhileBuffering(*engine_);
  GstEngineTest::SourceSetup(*engine_);
  EXPECT_FALSE(GstEngineTest::Buffering(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: a new source which ended buffering before the seek of Play() from an offset was applied started playback without the warm-up of the seek.
TEST_F(GstEngineSeekTest, SourceSetupKeepsWarmupOfQueuedSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::PlayFromOffset(*engine_, kFirstSeekNanosec);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::SourceSetup(*engine_);
  EXPECT_FALSE(GstEngineTest::Buffering(*engine_));
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: Play() on a prerolled pipeline started the warm-up while a delayed seek was not applied yet, so playback could start at the old position.
TEST_F(GstEngineSeekTest, PlayWhenPrerolledWaitsForDelayedSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::QueueTakenSeekDelayed(*engine_, GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec));
  GstEngineTest::Play(*engine_);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // The warm-up starts when the seek was applied.
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::LastKnownPosition(*engine_) == kFirstSeekNanosec; }));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Play() on a prerolled pipeline with a pending seek starts the warm-up after the seek was applied.
TEST_F(GstEngineSeekTest, PlayWhenPrerolledWaitsForPendingSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetPendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::Play(*engine_);
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // Only the queued calls are processed, not the messages of the pipeline.
  GstEngineTest::ProcessPendingSeek(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

}

// Regression test: a seek which was not applied, because the track changed since it was requested, started playback while Play() waited for a newer seek, so the old position was played first.
TEST_F(GstEngineSeekTest, SeekAfterTrackChangeKeepsPlaybackForTakenSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // Play() waits for the seek, which is taken, but not queued yet.
  const GstEngineTest::TakenSeek taken_seek = GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // A seek requested before a track change is not applied, and leaves starting playback to the taken seek.
  const int track_changes = GstEngineTest::TrackChanges(*engine_);
  GstEngineTest::ChangeTrack(*engine_);
  GstEngineTest::PipelineSeek(*engine_, kThirdSeekNanosec, track_changes);
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));

  // The taken seek starts the warm-up.
  GstEngineTest::QueueTakenSeek(*engine_, taken_seek);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: a seek which was ignored, like the seek to the start of a section, started playback while Play() waited for a newer seek, so the old position was played first.
TEST_F(GstEngineSeekTest, IgnoredSeekKeepsPlaybackForPendingSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // Play() waits for the pending seek.
  GstEngineTest::SetPendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // An ignored seek leaves starting playback to the pending seek.
  GstEngineTest::SetIgnoreNextSeek(*engine_);
  GstEngineTest::PipelineSeek(*engine_, kThirdSeekNanosec, -1);
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));

  // The pending seek starts the warm-up.
  GstEngineTest::ProcessPendingSeek(*engine_);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: buffering started with the state of an older transition to PLAYING while Play() waited for a seek, the pause for buffering was applied before the seek, and replaced the request to play after the seek.
TEST_F(GstEngineSeekTest, BufferingBeforeSeekKeepsPlaybackAfterSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // Play() waits for the seek, which is taken, but not queued yet, the message of an older transition to PLAYING is received, and buffering starts.
  const GstEngineTest::TakenSeek taken_seek = GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);

  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // The seek starts the warm-up.
  GstEngineTest::QueueTakenSeek(*engine_, taken_seek);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: buffering started with the state of an older transition to PLAYING while Play() waited for a seek, the pause for buffering was applied after the seek, and cancelled the warm-up.
TEST_F(GstEngineSeekTest, BufferingAfterSeekKeepsDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // Play() waits for the queued seek, the message of an older transition to PLAYING is received, and buffering starts, the seek is applied first.
  GstEngineTest::QueueTakenSeek(*engine_, GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec));
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  GstEngineTest::SetPipelinePlaying(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);

  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: a newer seek, which was taken on a streaming thread, but not queued yet, was not counted, so an older replaced seek which was applied in between started playback before the newer seek.
TEST_F(GstEngineSeekTest, ReplacedSeekWaitsForTakenSeek) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // The older seek is queued, Play() waits for it.
  GstEngineTest::QueueTakenSeek(*engine_, GstEngineTest::TakePendingSeek(*engine_, kThirdSeekNanosec));
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // The newer seek replaces it, and is taken, the older seek is applied before the newer seek is queued, only the queued calls are processed, not the messages of the pipeline.
  const GstEngineTest::TakenSeek taken_seek = GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_FALSE(GstEngineTest::DeviceWarmupWaiting(*engine_));

  // The newer seek starts playback after the warm-up.
  GstEngineTest::QueueTakenSeek(*engine_, taken_seek);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// A queued seek which Play() waits for, but which was replaced by a seek applied before Play(), still starts playback.
TEST_F(GstEngineSeekTest, ReplacedQueuedSeekStillStartsPlayback) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::QueueTakenSeek(*engine_, GstEngineTest::TakePendingSeek(*engine_, kFirstSeekNanosec));
  ASSERT_TRUE(GstEngineTest::PipelineSeek(*engine_, kThirdSeekNanosec, -1));
  GstEngineTest::Play(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));

  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kThirdSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_FALSE(GstEngineTest::DeviceWarmupAfterSeek(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));

}

// Regression test: buffering which finished during a seek reset, after Play() from an offset, started playback before the seek to the offset was applied.
TEST_F(GstEngineSeekTest, BufferingFinishedDuringSeekResetKeepsPlaybackForOffset) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);
  ASSERT_TRUE(GstEngineTest::NextUrlNeedsReset(*engine_));

  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec);
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

// Regression test: when buffering finished with a pending seek which was not applied, like the seek to the start of a section, playback was not resumed.
TEST_F(GstEngineSeekTest, BufferingFinishedResumesPlaybackWhenPendingSeekIsIgnored) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  // Buffering started while playing, and a seek is pending when it finished, which is ignored.
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::SetPendingState(*engine_, GST_STATE_PLAYING);
  GstEngineTest::SetPendingSeek(*engine_, kFirstSeekNanosec);
  GstEngineTest::SetIgnoreNextSeek(*engine_);
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // Only the queued calls are processed, not the messages of the pipeline.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_EQ(kSecondSeekNanosec, GstEngineTest::LastKnownPosition(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: a buffering message received after a new source was set up, but before buffering was finished for it, was ended by the source, while the queue was not filled.
TEST_F(GstEngineSeekTest, BufferingMessageAfterSourceSetupKeepsBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::SourceSetupQueued(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 50);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_TRUE(GstEngineTest::Buffering(*engine_));

}

// Regression test: buffering which finished and started again after a new source was set up, before buffering was finished for it, was ended by the source.
TEST_F(GstEngineSeekTest, BufferingStartedAgainAfterSourceSetupKeepsBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::PlayFromOffset(*engine_, kSecondSeekNanosec, true);
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::PipelineState(*engine_) == GST_STATE_PAUSED && GstEngineTest::LastKnownPosition(*engine_) == kSecondSeekNanosec && !GstEngineTest::Buffering(*engine_); }));

  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::SourceSetupQueued(*engine_);
  GstEngineTest::BufferingFinished(*engine_);
  GstEngineTest::BufferingMessage(*engine_, 30);
  ASSERT_TRUE(GstEngineTest::Buffering(*engine_));
  QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  EXPECT_TRUE(GstEngineTest::Buffering(*engine_));

}

// Regression test: when buffering after going to READY for a seek reset, the source for the next track ended buffering and started playback without the audio device warm-up.
TEST_F(GstEngineSeekTest, ResumeAfterSeekResetWhileBufferingWaitsForDeviceWarmup) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 500);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);

  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::PipelineReady(*engine_);
  ASSERT_TRUE(GstEngineTest::DeviceWarmupPending(*engine_));

  // The pipeline prerolls with a new source, which ends buffering, and the warm-up starts, playback is not requested before it.
  ASSERT_TRUE(WaitForPipeline([this]() { return GstEngineTest::DeviceWarmupWaiting(*engine_) || GstEngineTest::TargetState(*engine_) == GST_STATE_PLAYING; }));
  EXPECT_FALSE(GstEngineTest::Buffering(*engine_));
  EXPECT_TRUE(GstEngineTest::DeviceWarmupWaiting(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));

  // Playback starts when the warm-up elapsed.
  // The track is short, so when it plays, it can play to the end, and the pipeline is removed.
  EXPECT_TRUE(WaitFor([this]() { return PlaybackStarted(); }));

}

// Regression test: playback was resumed after going to READY while buffering, so it played while still buffering.
TEST_F(GstEngineSeekTest, ResumeAfterSeekResetWaitsForBuffering) {

  ASSERT_TRUE(Load());
  GstEngineTest::SetDeviceWarmup(*engine_, 0);
  GstEngineTest::SeekWhileNextUrlIsSet(*engine_, url_, kFirstSeekNanosec, GST_STATE_PLAYING);

  GstEngineTest::StartNextStream(*engine_);
  GstEngineTest::SetBuffering(*engine_, true);
  GstEngineTest::PipelineReady(*engine_);

  // Playback is resumed when buffering finished.
  EXPECT_FALSE(GstEngineTest::NextUrlNeedsReset(*engine_));
  EXPECT_EQ(GST_STATE_PAUSED, GstEngineTest::TargetState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::PendingState(*engine_));
  GstEngineTest::BufferingFinished(*engine_);
  EXPECT_EQ(GST_STATE_NULL, GstEngineTest::PendingState(*engine_));
  EXPECT_EQ(GST_STATE_PLAYING, GstEngineTest::TargetState(*engine_));

}

// Regression test: the buffer probe moved on to the next section of a multipart file while a seek for the current section was being submitted.
TEST_F(GstEngineSeekTest, SectionChangeWaitsForSeek) {

  ASSERT_TRUE(Load());
  const int track_changes = GstEngineTest::TrackChanges(*engine_);

  // Locked like while a seek is submitted, the section is not changed for this buffer.
  GstEngineTest::LockSeekSection(*engine_);
  GstEngineTest::ProbeBufferAfterSectionEnd(*engine_, url_, kFirstSeekNanosec);
  GstEngineTest::UnlockSeekSection(*engine_);
  EXPECT_EQ(track_changes, GstEngineTest::TrackChanges(*engine_));

  // The next buffer after the seek changes it.
  GstEngineTest::ProbeBufferAfterSectionEnd(*engine_, url_, kFirstSeekNanosec);
  EXPECT_EQ(track_changes + 1, GstEngineTest::TrackChanges(*engine_));

}

// Regression test: a seek waiting when the pipeline moved on to the next track in gapless playback was applied to the next track.
TEST_F(GstEngineSeekTest, WaitingSeekIsNotAppliedAfterGaplessTrackChange) {

  ASSERT_TRUE(Load());
  engine_->Seek(kFirstSeekNanosec);
  engine_->Seek(kSecondSeekNanosec);

  GstEngineTest::StartNextTrack(*engine_, url_);

  ProcessEventsForSeekDelay();

  EXPECT_EQ(kFirstSeekNanosec, GstEngineTest::PendingSeek(*engine_));

}

}  // namespace
