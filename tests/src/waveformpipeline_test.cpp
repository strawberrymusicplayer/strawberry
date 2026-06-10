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
#include <QThread>
#include <QSignalSpy>

#include "engine/gststartup.h"
#include "waveform/waveformpipeline.h"

#include "test_utils.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Decodes the bundled WAV through the real GStreamer pipeline on a dedicated worker thread (the pipeline asserts it does not run on the qApp thread) and confirms a versioned magic blob is produced on EOS.
TEST(WaveformPipelineTest, DecodesWavToMagicBlob) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  WaveformPipeline pipeline(url);
  QSignalSpy spy(&pipeline, &WaveformPipeline::Finished);

  // Start() asserts QThread::currentThread() != qApp->thread(), so drive it from a dedicated worker thread, mirroring MoodbarLoader.
  QThread worker_thread;
  pipeline.moveToThread(&worker_thread);
  worker_thread.start();

  QMetaObject::invokeMethod(&pipeline, &WaveformPipeline::Start, Qt::QueuedConnection);

  const bool finished = spy.wait(10000);
  worker_thread.quit();
  worker_thread.wait();
  ASSERT_TRUE(finished);

  EXPECT_EQ(spy.count(), 1);
  EXPECT_TRUE(pipeline.success());

  const QByteArray data = pipeline.data();
  EXPECT_GT(data.size(), 16);
  EXPECT_EQ(data.left(4), QByteArray("SWVF"));

}

}  // namespace
