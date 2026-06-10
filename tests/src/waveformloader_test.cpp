/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#include "gtest_include.h"
#include "gmock_include.h"

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>
#include <QSignalSpy>

#include "engine/gststartup.h"
#include "core/standardpaths.h"
#include "waveform/waveformloader.h"
#include "waveform/waveformpipeline.h"

#include "test_utils.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Drives the real async loader against the bundled WAV from an isolated empty
// cache (QStandardPaths test mode), covering in-flight URL dedupe (EXT-03) and
// cache reuse on replay with no second decode (STO-03).
class WaveformLoaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    GstStartup::Initialize();

    // Isolate the cache: redirect CacheLocation to a per-user test path BEFORE
    // constructing the loader (the loader fixes its cache dir in its ctor).
    QStandardPaths::setTestModeEnabled(true);

    // Guarantee a miss on the first Load even across repeated test runs.
    const QString waveform_cache_dir = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + u"/waveform"_s;
    QDir(waveform_cache_dir).removeRecursively();
  }
};

TEST_F(WaveformLoaderTest, DedupesInFlightAndReusesCacheOnReplay) {

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  WaveformLoader loader;

  // First Load on an empty cache must take the async generation path (EXT-03).
  WaveformLoader::LoadResult first = loader.Load(url, false);
  ASSERT_EQ(first.status, WaveformLoader::LoadStatus::WillLoadAsync);
  ASSERT_TRUE(first.pipeline);

  // A second Load while the first is in flight returns the SAME pipeline
  // (EXT-03 in-flight URL dedupe).
  WaveformLoader::LoadResult duplicate = loader.Load(url, false);
  EXPECT_EQ(duplicate.status, WaveformLoader::LoadStatus::WillLoadAsync);
  EXPECT_EQ(duplicate.pipeline.data(), first.pipeline.data());

  // Wait for generation to complete.
  QSignalSpy spy(&*first.pipeline, &WaveformPipeline::Finished);
  ASSERT_TRUE(spy.wait(10000));
  EXPECT_EQ(spy.count(), 1);

  // A later Load returns Loaded synchronously from the cache - no second decode
  // (STO-03 cache reuse).
  WaveformLoader::LoadResult replay = loader.Load(url, false);
  EXPECT_EQ(replay.status, WaveformLoader::LoadStatus::Loaded);
  EXPECT_GT(replay.data.size(), 16);
  EXPECT_EQ(replay.data.left(4), QByteArray("SWVF"));

}

}  // namespace
