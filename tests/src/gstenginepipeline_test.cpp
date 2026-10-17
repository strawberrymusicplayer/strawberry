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

#include "config.h"

#include <gst/gst.h>

#include <QtGlobal>

#ifndef Q_OS_WIN32
#  include <unistd.h>
#  include <csignal>
#  include <thread>
#endif

#include "gtest_include.h"

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>

#include "engine/gststartup.h"
#include "engine/gstenginepipeline.h"

#include "test_utils.h"

using namespace Qt::Literals::StringLiterals;

// Declared at file scope so that GstEnginePipeline's "friend class GstEnginePipelineTest;" resolves to this class.
class GstEnginePipelineTest {
 public:
  // Waits until the GStreamer pipeline itself is in the state, without processing the bus messages, so the cached state in the GstEnginePipeline is not updated.
  static bool WaitForPipelineState(GstEnginePipeline &pipeline, const GstState state, const int timeout_msec = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout_msec) {
      GstState current_state = GST_STATE_NULL;
      if (gst_element_get_state(pipeline.pipeline_, &current_state, nullptr, 100 * GST_MSECOND) == GST_STATE_CHANGE_SUCCESS && current_state == state) {
        return true;
      }
    }
    return false;
  }

  static void StartPlaybackAfterWarmup(GstEnginePipeline &pipeline) {
    pipeline.StartPlaybackAfterWarmup();
  }

  static GstState TargetState(const GstEnginePipeline &pipeline) {
    return pipeline.target_state_.load();
  }

  // Runs the current warm-up timer callback now.
  static void DeviceWarmupElapsed(GstEnginePipeline &pipeline) {
    pipeline.DeviceWarmupElapsed(pipeline.device_warmup_generation_.load());
  }
};

namespace {

// Processes Qt events, and the default GLib context, which receives the pipeline bus messages, Qt only processes it when it uses the GLib event dispatcher.
void ProcessEvents() {
  QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  while (g_main_context_iteration(nullptr, FALSE)) {}
}

// Processes events until the condition is true, returns false on timeout.
template <typename Condition>
bool WaitFor(Condition condition, const int timeout_msec = 10000) {
  QElapsedTimer timer;
  timer.start();
  while (!condition()) {
    if (timer.elapsed() > timeout_msec) return false;
    ProcessEvents();
  }
  return true;
}

// Processes events for the duration.
void ProcessEventsFor(const int duration_msec) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < duration_msec) {
    ProcessEvents();
  }
}

// Regression test: pausing while playback was waiting for the audio device warm-up was ignored, and the warm-up timer then started playback anyway.
TEST(GstEnginePipelineTest, PauseDuringDeviceWarmupHoldsPlayback) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  pipeline.set_device_warmup_duration_ms(500);
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

  pipeline.Play(false, 0);
  EXPECT_TRUE(pipeline.play_pending());

  // Pause while the pipeline is prerolling or waiting for the warm-up.
  ASSERT_TRUE(WaitFor([&pipeline]() { return pipeline.state() == GST_STATE_PAUSED; }));
  ASSERT_NE(GST_STATE_PLAYING, pipeline.state());
  pipeline.PauseBeforePlaying();
  EXPECT_TRUE(pipeline.paused_before_playing());

  // The warm-up has elapsed, but playback is held.
  ProcessEventsFor(1000);
  EXPECT_EQ(GST_STATE_PAUSED, pipeline.state());

  // Unpausing starts playback.
  pipeline.UnpauseBeforePlaying();
  EXPECT_FALSE(pipeline.paused_before_playing());
  EXPECT_TRUE(WaitFor([&pipeline]() { return pipeline.state() == GST_STATE_PLAYING; }));

}

// Regression test: without a warm-up, playback goes straight to PLAYING, and pausing before the pipeline was PLAYING was ignored.
TEST(GstEnginePipelineTest, PauseDuringDirectStartHoldsPlayback) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  pipeline.set_device_warmup_duration_ms(0);
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

  // Pause while going to PLAYING is in progress.
  pipeline.Play(false, 0);
  ASSERT_NE(GST_STATE_PLAYING, pipeline.state());
  EXPECT_TRUE(pipeline.play_pending());
  pipeline.PauseBeforePlaying();

  ProcessEventsFor(1000);
  EXPECT_EQ(GST_STATE_PAUSED, pipeline.state());

  // Unpausing starts playback.
  pipeline.UnpauseBeforePlaying();
  EXPECT_TRUE(WaitFor([&pipeline]() { return pipeline.state() == GST_STATE_PLAYING; }));

}

// Regression test: playback was not pending anymore after the warm-up delay elapsed, before the pipeline was PLAYING, so pausing then was ignored.
TEST(GstEnginePipelineTest, PlayPendingUntilPlaying) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  for (const int device_warmup_duration_ms : {0, 300}) {
    GstEnginePipeline pipeline;
    pipeline.set_output_device(u"fakesink"_s, QVariant());
    pipeline.set_device_warmup_duration_ms(device_warmup_duration_ms);
    QString error;
    ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

    pipeline.Play(false, 0);
    bool always_pending = true;
    EXPECT_TRUE(WaitFor([&pipeline, &always_pending]() {
      if (pipeline.state() == GST_STATE_PLAYING) return true;
      // Pausing is not possible while buffering, so playback is not pending then.
      if (!pipeline.play_pending() && !pipeline.is_buffering()) always_pending = false;
      return false;
    }));
    EXPECT_TRUE(always_pending) << "Warm-up " << device_warmup_duration_ms << " ms";
  }

}

// Regression test: when the pipeline was PLAYING before the pause requested in PauseBeforePlaying() was applied, unpausing before the pause was applied did nothing, since the state was still PLAYING, so playback stayed paused.
TEST(GstEnginePipelineTest, UnpauseBeforePauseIsAppliedResumesPlayback) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  pipeline.set_device_warmup_duration_ms(0);
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

  // Let the pipeline reach PLAYING without processing the state messages, then pause while going to PLAYING is still in progress for the pipeline.
  pipeline.Play(false, 0);
  ASSERT_TRUE(GstEnginePipelineTest::WaitForPipelineState(pipeline, GST_STATE_PLAYING));
  ASSERT_NE(GST_STATE_PLAYING, pipeline.state());
  ASSERT_TRUE(pipeline.play_pending());
  pipeline.PauseBeforePlaying();
  ASSERT_TRUE(GstEnginePipelineTest::WaitForPipelineState(pipeline, GST_STATE_PAUSED));

  // Process the bus messages one by one until the PLAYING state message is processed, the PAUSED state message is processed after it.
  // The test drives the GLib default context on the pipeline's thread, so the bus messages are handled directly on all platforms, they are only posted as Qt events when a separate GLib thread drives the context.
  QElapsedTimer timer;
  timer.start();
  while (pipeline.state() != GST_STATE_PLAYING && timer.elapsed() < 10000) {
    g_main_context_iteration(nullptr, FALSE);
  }
  ASSERT_EQ(GST_STATE_PLAYING, pipeline.state());
  // Still paused, so the engine reports it as paused, and unpauses it.
  EXPECT_TRUE(pipeline.paused_before_playing());

  // Unpause before the PAUSED state message is processed.
  pipeline.UnpauseBeforePlaying();

  // The queued PAUSED state message from the pause is processed before the PLAYING state message from unpausing, playback is still pending then, so pausing is not missed.
  timer.restart();
  while (pipeline.state() != GST_STATE_PAUSED && timer.elapsed() < 10000) {
    g_main_context_iteration(nullptr, FALSE);
  }
  ASSERT_EQ(GST_STATE_PAUSED, pipeline.state());
  EXPECT_TRUE(pipeline.play_pending());

  // The pause is applied, then playback is resumed, and playback is pending until PLAYING is received, so pausing in between is not missed.
  bool always_pending = true;
  EXPECT_TRUE(WaitFor([&pipeline, &always_pending]() {
    if (pipeline.state() == GST_STATE_PLAYING) return true;
    if (!pipeline.play_pending() && !pipeline.is_buffering()) always_pending = false;
    return false;
  }));
  EXPECT_TRUE(always_pending);
  ProcessEventsFor(500);
  EXPECT_EQ(GST_STATE_PLAYING, pipeline.state());

}

// Regression test: a request to go to PLAYING which was superseded by a request to go to PAUSED, like a pause while going back to PLAYING after buffering, stayed pending, so the paused pipeline was reported as playing.
TEST(GstEnginePipelineTest, SupersededPlayingRequestIsNotPending) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  pipeline.set_device_warmup_duration_ms(0);
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

  pipeline.Play(false, 0);
  ASSERT_TRUE(pipeline.play_pending());
  pipeline.SetState(GST_STATE_PAUSED);

  ProcessEventsFor(1000);
  EXPECT_EQ(GST_STATE_PAUSED, pipeline.state());
  EXPECT_FALSE(pipeline.play_pending());

}

// Regression test: the PLAYING state message from before pausing cleared the request to go to PLAYING when unpausing, so playback was not pending after the PAUSED state message from the pause, and pausing was missed.
TEST(GstEnginePipelineTest, StaleStateMessagesKeepPlaybackPending) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  pipeline.set_device_warmup_duration_ms(0);
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

  // Let the pipeline reach PLAYING, then PAUSED, without processing the state messages, and unpause before any of them are processed.
  pipeline.Play(false, 0);
  ASSERT_TRUE(GstEnginePipelineTest::WaitForPipelineState(pipeline, GST_STATE_PLAYING));
  pipeline.PauseBeforePlaying();
  ASSERT_TRUE(GstEnginePipelineTest::WaitForPipelineState(pipeline, GST_STATE_PAUSED));
  pipeline.UnpauseBeforePlaying();
  EXPECT_TRUE(pipeline.play_pending());

  // Process the state messages one by one, playback is pending until the PLAYING state message from unpausing is processed.
  // Unpausing requests PLAYING with a queued call to the pipeline, which is only delivered by iterating the GLib context when Qt uses the GLib event dispatcher, so also deliver the queued calls to the pipeline, they don't process bus messages.
  bool seen_playing = false;
  bool seen_paused_after_playing = false;
  bool always_pending = true;
  QElapsedTimer timer;
  timer.start();
  while (!(seen_paused_after_playing && pipeline.state() == GST_STATE_PLAYING) && timer.elapsed() < 10000) {
    QCoreApplication::sendPostedEvents(&pipeline, QEvent::MetaCall);
    g_main_context_iteration(nullptr, FALSE);
    if (pipeline.state() == GST_STATE_PLAYING) {
      seen_playing = true;
    }
    else if (seen_playing && pipeline.state() == GST_STATE_PAUSED) {
      seen_paused_after_playing = true;
      if (!pipeline.play_pending() && !pipeline.is_buffering()) always_pending = false;
    }
  }
  EXPECT_TRUE(seen_paused_after_playing);
  EXPECT_TRUE(always_pending);
  EXPECT_EQ(GST_STATE_PLAYING, pipeline.state());

}

// Regression test: a state change cancelled the warm-up timer, but playback stayed pending until the timer elapsed, so the paused pipeline was reported as playing.
TEST(GstEnginePipelineTest, CancelledWarmupIsNotPending) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  pipeline.set_device_warmup_duration_ms(2000);
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

  // Wait until the pipeline is prerolled and waiting for the warm-up.
  pipeline.Play(false, 0);
  ASSERT_TRUE(WaitFor([&pipeline]() { return pipeline.state() == GST_STATE_PAUSED; }));
  ASSERT_TRUE(pipeline.play_pending());

  // Pause, like a normal pause, which cancels the warm-up, playback is not pending, also before the warm-up timer elapsed.
  pipeline.SetState(GST_STATE_PAUSED);
  EXPECT_FALSE(pipeline.play_pending());
  ProcessEventsFor(500);
  EXPECT_FALSE(pipeline.play_pending());
  EXPECT_EQ(GST_STATE_PAUSED, pipeline.state());

  // Resuming starts a new warm-up, the old warm-up timer elapses before it, and does not clear it.
  GstEnginePipelineTest::StartPlaybackAfterWarmup(pipeline);
  EXPECT_TRUE(pipeline.play_pending());
  ProcessEventsFor(1700);
  EXPECT_EQ(GST_STATE_PAUSED, pipeline.state());
  EXPECT_TRUE(pipeline.play_pending());
  EXPECT_TRUE(WaitFor([&pipeline]() { return pipeline.state() == GST_STATE_PLAYING; }));

}

// Regression test: when unpausing quickly with a warm-up, before the pause was applied, the warm-up timer did not start playback when the PLAYING state message from before the pause was processed but not the PAUSED state message, so playback stayed paused.
TEST(GstEnginePipelineTest, WarmupAfterQuickUnpauseResumesPlayback) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  // The warm-up timers don't elapse during the test, the test elapses the warm-up itself, so it controls the order of the state messages.
  pipeline.set_device_warmup_duration_ms(600000);
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

  // Wait for the preroll, elapse the warm-up, which requests PLAYING, then apply the request without processing the bus messages, and let the pipeline reach PLAYING.
  pipeline.Play(false, 0);
  ASSERT_TRUE(WaitFor([&pipeline]() { return pipeline.state() == GST_STATE_PAUSED && pipeline.play_pending(); }));
  GstEnginePipelineTest::DeviceWarmupElapsed(pipeline);
  ASSERT_EQ(GST_STATE_PLAYING, GstEnginePipelineTest::TargetState(pipeline));
  ASSERT_EQ(GST_STATE_PAUSED, pipeline.state());
  QCoreApplication::sendPostedEvents(&pipeline, QEvent::MetaCall);
  ASSERT_TRUE(GstEnginePipelineTest::WaitForPipelineState(pipeline, GST_STATE_PLAYING));

  // Pause and unpause before the state messages are processed, unpausing starts a new warm-up.
  pipeline.PauseBeforePlaying();
  ASSERT_TRUE(GstEnginePipelineTest::WaitForPipelineState(pipeline, GST_STATE_PAUSED));
  ASSERT_EQ(GST_STATE_PAUSED, pipeline.state());
  pipeline.UnpauseBeforePlaying();
  EXPECT_TRUE(pipeline.play_pending());

  // Process the PLAYING state message from before the pause, but not the PAUSED state message, then the warm-up elapses.
  QElapsedTimer timer;
  timer.start();
  while (pipeline.state() != GST_STATE_PLAYING && timer.elapsed() < 10000) {
    g_main_context_iteration(nullptr, FALSE);
  }
  ASSERT_EQ(GST_STATE_PLAYING, pipeline.state());
  GstEnginePipelineTest::DeviceWarmupElapsed(pipeline);

  // The PAUSED state message is processed, playback is still pending then, and playback is resumed.
  // Process the bus messages one by one, and deliver the queued calls to the pipeline, which request PLAYING.
  bool seen_paused = false;
  bool always_pending = true;
  timer.restart();
  while (!(seen_paused && pipeline.state() == GST_STATE_PLAYING) && timer.elapsed() < 10000) {
    QCoreApplication::sendPostedEvents(&pipeline, QEvent::MetaCall);
    g_main_context_iteration(nullptr, FALSE);
    if (pipeline.state() == GST_STATE_PAUSED) {
      seen_paused = true;
      if (!pipeline.play_pending() && !pipeline.is_buffering()) always_pending = false;
    }
  }
  EXPECT_TRUE(seen_paused);
  EXPECT_TRUE(always_pending);
  EXPECT_EQ(GST_STATE_PLAYING, pipeline.state());

}

#ifndef Q_OS_WIN32
// Regression test: when the seek to the offset failed when starting playback, like for a stream which is not seekable, playback never started, and pausing and unpausing did not start it either.
TEST(GstEnginePipelineTest, FailedStartupSeekStartsPlayback) {

  GstStartup::Initialize();

  QFile wav_file(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(wav_file.open(QIODevice::ReadOnly));
  const QByteArray wav_data = wav_file.readAll();
  ASSERT_FALSE(wav_data.isEmpty());

  // A pipe is not seekable, so the seek to the offset fails.
  int pipe_fds[2] = { -1, -1 };
  ASSERT_EQ(0, pipe(pipe_fds));

  // Write the file to the pipe, writing fails when the pipe is closed for reading, SIGPIPE is ignored, so it fails with EPIPE instead.
  std::signal(SIGPIPE, SIG_IGN);
  std::thread writer([write_fd = pipe_fds[1], wav_data]() {
    qint64 written = 0;
    while (written < wav_data.size()) {
      const ssize_t result = write(write_fd, wav_data.constData() + written, static_cast<size_t>(wav_data.size() - written));
      if (result <= 0) break;
      written += result;
    }
    close(write_fd);
  });

  // The pipeline is tested in a lambda, so a fatal assertion only returns from the lambda, and the writer is still stopped and joined below.
  const auto test_playback = [&pipe_fds]() {
    const QByteArray gst_url = "fd://" + QByteArray::number(pipe_fds[0]);
    const QUrl url(QString::fromLatin1(gst_url));
    GstEnginePipeline pipeline;
    pipeline.set_output_device(u"fakesink"_s, QVariant());
    pipeline.set_device_warmup_duration_ms(0);
    QString error;
    ASSERT_TRUE(pipeline.InitFromUrl(url, url, gst_url, 0, 0, 0.0, error)) << error.toStdString();

    // Start playback from an offset, and pause while it is prerolling, the pause holds playback when the seek fails, and unpausing starts it.
    pipeline.Play(false, 1 * GST_SECOND);
    EXPECT_TRUE(pipeline.play_pending());
    pipeline.PauseBeforePlaying();
    ProcessEventsFor(1000);
    EXPECT_EQ(GST_STATE_PAUSED, pipeline.state());
    pipeline.UnpauseBeforePlaying();
    EXPECT_TRUE(WaitFor([&pipeline]() { return pipeline.state() == GST_STATE_PLAYING; }));
    EXPECT_FALSE(pipeline.play_pending());
  };
  test_playback();

  // Close the read end of the pipe, so the writer stops if it has not written everything.
  close(pipe_fds[0]);
  writer.join();

}
#endif

}  // namespace
