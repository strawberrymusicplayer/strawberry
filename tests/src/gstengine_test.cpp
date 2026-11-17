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

#include <memory>

#include "gtest_include.h"

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <QFile>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
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
  static bool PipelineSeek(GstEngine &engine, const qint64 nanosec, const int expected_track_changes) { return engine.current_pipeline_->Seek(nanosec, expected_track_changes); }
  // Retries the pending seek like when the pipeline became active, the retry is queued.
  static void ProcessPendingSeek(GstEngine &engine) { engine.current_pipeline_->ProcessPendingSeek(GST_STATE_PAUSED); }
  // Like after the next URL was reverted for the seek, the retry is delayed.
  static void ProcessPendingSeekAfterNextUrlReset(GstEngine &engine) {
    engine.current_pipeline_->next_uri_reset_ = true;
    engine.current_pipeline_->pending_seek_ready_previous_state_ = GST_STATE_PAUSED;
    engine.current_pipeline_->ProcessPendingSeek(GST_STATE_PAUSED);
  }
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

  TemporaryResource resource_{u":/audio/strawberry.wav"_s};
  QUrl url_;
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
