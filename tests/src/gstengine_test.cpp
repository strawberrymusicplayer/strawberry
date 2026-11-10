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

}  // namespace
