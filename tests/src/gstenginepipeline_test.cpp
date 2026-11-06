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

#include <initializer_list>
#include <utility>

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
#include <QList>
#include <QMutexLocker>

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

  // Sets whether the pipeline is buffering, like a buffering message from the queue does.
  static void SetBuffering(GstEnginePipeline &pipeline, const bool buffering) {
    pipeline.buffering_ = buffering;
  }

  /**
   * Collects every MetadataFound bundle emitted by pipeline in metadata.
   * Keep metadata alive until this connection is disconnected, normally when pipeline is destroyed.
   */
  static void CaptureMetadata(GstEnginePipeline &pipeline, QList<EngineMetadata> &metadata) {
    QObject::connect(&pipeline, &GstEnginePipeline::MetadataFound, &pipeline, [&metadata](const int, const EngineMetadata &bundle) { metadata.append(bundle); });
  }

  /**
   * Builds and synchronously dispatches a synthetic GST_MESSAGE_TAG containing strings and, when nonnegative, bitrate.
   * String values are UTF-8 encoded, bitrate is in bits per second, zero is included, and a negative value omits the bitrate tag.
   * The message takes ownership of its tag list and is unreferenced after dispatch.
   * @param pipeline Pipeline whose bus-message handler receives the synthetic message.
   * @param strings String tags to include in the message.
   * @param bitrate Bitrate in bits per second, or a negative value to omit the tag.
   */
  static void DispatchTags(GstEnginePipeline &pipeline, const std::initializer_list<std::pair<const char *, QString>> &strings = {}, const int bitrate = -1) {
    GstTagList *tags = gst_tag_list_new_empty();
    for (const auto &[tag, value] : strings) {
      const QByteArray utf8 = value.toUtf8();
      gst_tag_list_add(tags, GST_TAG_MERGE_REPLACE, tag, utf8.isNull() ? "" : utf8.constData(), nullptr);
    }
    if (bitrate >= 0) gst_tag_list_add(tags, GST_TAG_MERGE_REPLACE, GST_TAG_BITRATE, static_cast<guint>(bitrate), nullptr);

    // gst_message_new_tag() takes ownership of the tag list; HandleBusMessage() does not own the message.
    GstMessage *message = gst_message_new_tag(nullptr, tags);
    pipeline.HandleBusMessage(message);
    gst_message_unref(message);
  }

  /**
   * Dispatches and unreferences a synthetic GST_MESSAGE_STREAM_START to exercise stream transition and metadata-reset handling.
   * This calls the handler directly and does not start playback or create a real stream.
   */
  static void DispatchStreamStart(GstEnginePipeline &pipeline) {
    GstMessage *message = gst_message_new_stream_start(nullptr);
    pipeline.HandleBusMessage(message);
    gst_message_unref(message);
  }

  /**
   * Sets the current URLs reported with metadata while holding the pipeline's URL mutex.
   * This only prepares synthetic-test state and does not initialize the pipeline or start playback.
   */
  static void SetCurrentUrls(GstEnginePipeline &pipeline, const QUrl &media_url, const QUrl &stream_url) {
    QMutexLocker lock(&pipeline.mutex_url_);
    pipeline.media_url_ = media_url;
    pipeline.stream_url_ = stream_url;
  }

  /**
   * Prepares next-track URLs and the GStreamer URI under the next-URL mutex, then marks the URI pending for stream-start promotion.
   * This simulates queued gapless state without preloading a pipeline or starting playback.
   */
  static void SetNextUrlsAndMarkSet(GstEnginePipeline &pipeline, const QUrl &media_url, const QUrl &stream_url) {
    {
      QMutexLocker lock(&pipeline.mutex_next_url_);
      pipeline.next_media_url_ = media_url;
      pipeline.next_stream_url_ = stream_url;
      pipeline.next_gst_url_ = stream_url.toEncoded();
    }
    pipeline.next_uri_set_ = true;
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

TEST(GstEnginePipelineTest, PartialTagUpdatesPreserveMetadataAndExplicitTagsWin) {

  GstStartup::Initialize();

  const QUrl media_url(u"https://example.test/media"_s);
  const QUrl stream_url(u"https://example.test/stream"_s);

  QList<EngineMetadata> forward_metadata;
  GstEnginePipeline forward_pipeline;
  GstEnginePipelineTest::SetCurrentUrls(forward_pipeline, media_url, stream_url);
  GstEnginePipelineTest::CaptureMetadata(forward_pipeline, forward_metadata);

  GstEnginePipelineTest::DispatchTags(forward_pipeline, {{GST_TAG_ARTIST, u"Explicit Artist"_s}, {GST_TAG_ALBUM, u"Explicit Album"_s}});
  GstEnginePipelineTest::DispatchTags(forward_pipeline, {{GST_TAG_TITLE, uR"({"title":"JSON Title","artist":"JSON Artist","album":"JSON Album"})"_s}});

  ASSERT_EQ(2, forward_metadata.size());
  EXPECT_EQ(u"JSON Title"_s, forward_metadata[1].title);
  EXPECT_EQ(u"Explicit Artist"_s, forward_metadata[1].artist);
  EXPECT_EQ(u"Explicit Album"_s, forward_metadata[1].album);
  EXPECT_EQ(EngineMetadata::Type::Current, forward_metadata[1].type);
  EXPECT_EQ(media_url, forward_metadata[1].media_url);
  EXPECT_EQ(stream_url, forward_metadata[1].stream_url);

  QList<EngineMetadata> reverse_metadata;
  GstEnginePipeline reverse_pipeline;
  GstEnginePipelineTest::SetCurrentUrls(reverse_pipeline, media_url, stream_url);
  GstEnginePipelineTest::CaptureMetadata(reverse_pipeline, reverse_metadata);

  GstEnginePipelineTest::DispatchTags(reverse_pipeline, {{GST_TAG_TITLE, uR"({"title":"JSON Title","artist":"JSON Artist","album":"JSON Album"})"_s}});
  GstEnginePipelineTest::DispatchTags(reverse_pipeline, {{GST_TAG_ARTIST, u"Explicit Artist"_s}, {GST_TAG_ALBUM, u"Explicit Album"_s}});
  GstEnginePipelineTest::DispatchTags(reverse_pipeline, {{GST_TAG_COMMENT, u"Station comment"_s}, {GST_TAG_LYRICS, u"Lyrics"_s}}, 128000);

  ASSERT_EQ(3, reverse_metadata.size());
  const EngineMetadata &metadata = reverse_metadata[2];
  EXPECT_EQ(u"JSON Title"_s, metadata.title);
  EXPECT_EQ(u"Explicit Artist"_s, metadata.artist);
  EXPECT_EQ(u"Explicit Album"_s, metadata.album);
  EXPECT_EQ(u"Station comment"_s, metadata.comment);
  EXPECT_EQ(u"Lyrics"_s, metadata.lyrics);
  EXPECT_EQ(128, metadata.bitrate);
  EXPECT_EQ(EngineMetadata::Type::Current, metadata.type);
  EXPECT_EQ(media_url, metadata.media_url);
  EXPECT_EQ(stream_url, metadata.stream_url);

  GstEnginePipelineTest::DispatchTags(reverse_pipeline, {}, 192001);
  ASSERT_EQ(4, reverse_metadata.size());
  EXPECT_EQ(u"JSON Title"_s, reverse_metadata[3].title);
  EXPECT_EQ(u"Explicit Artist"_s, reverse_metadata[3].artist);
  EXPECT_EQ(u"Explicit Album"_s, reverse_metadata[3].album);
  EXPECT_EQ(u"Station comment"_s, reverse_metadata[3].comment);
  EXPECT_EQ(u"Lyrics"_s, reverse_metadata[3].lyrics);
  EXPECT_EQ(192, reverse_metadata[3].bitrate);

  GstEnginePipelineTest::DispatchTags(reverse_pipeline, {}, 0);
  GstEnginePipelineTest::DispatchTags(reverse_pipeline);
  ASSERT_EQ(6, reverse_metadata.size());
  EXPECT_EQ(reverse_metadata[4].title, reverse_metadata[5].title);
  EXPECT_EQ(reverse_metadata[4].artist, reverse_metadata[5].artist);
  EXPECT_EQ(reverse_metadata[4].album, reverse_metadata[5].album);
  EXPECT_EQ(reverse_metadata[4].comment, reverse_metadata[5].comment);
  EXPECT_EQ(reverse_metadata[4].lyrics, reverse_metadata[5].lyrics);
  EXPECT_EQ(0, reverse_metadata[4].bitrate);
  EXPECT_EQ(0, reverse_metadata[5].bitrate);
  EXPECT_EQ(EngineMetadata::Type::Current, reverse_metadata[5].type);
  EXPECT_EQ(media_url, reverse_metadata[5].media_url);
  EXPECT_EQ(stream_url, reverse_metadata[5].stream_url);

}

TEST(GstEnginePipelineTest, ReplacedTitlesReparseJsonAndLegacyMetadata) {

  GstStartup::Initialize();

  QList<EngineMetadata> metadata;
  GstEnginePipeline pipeline;
  GstEnginePipelineTest::CaptureMetadata(pipeline, metadata);

  GstEnginePipelineTest::DispatchTags(pipeline, {{GST_TAG_TITLE, uR"({"title":"First title","artist":"First artist","album":"First album"})"_s}});
  GstEnginePipelineTest::DispatchTags(pipeline, {{GST_TAG_TITLE, uR"({"title":"Second title","artist":"Second artist","album":"Second album"})"_s}});
  GstEnginePipelineTest::DispatchTags(pipeline, {{GST_TAG_TITLE, u"A - T - Album"_s}});
  GstEnginePipelineTest::DispatchTags(pipeline, {{GST_TAG_TITLE, u"B - U - Other"_s}});

  ASSERT_EQ(4, metadata.size());
  EXPECT_EQ(u"First title"_s, metadata[0].title);
  EXPECT_EQ(u"First artist"_s, metadata[0].artist);
  EXPECT_EQ(u"First album"_s, metadata[0].album);
  EXPECT_EQ(u"Second title"_s, metadata[1].title);
  EXPECT_EQ(u"Second artist"_s, metadata[1].artist);
  EXPECT_EQ(u"Second album"_s, metadata[1].album);
  EXPECT_EQ(u"T"_s, metadata[2].title);
  EXPECT_EQ(u"A"_s, metadata[2].artist);
  EXPECT_EQ(u"Album"_s, metadata[2].album);
  EXPECT_EQ(u"U"_s, metadata[3].title);
  EXPECT_EQ(u"B"_s, metadata[3].artist);
  EXPECT_EQ(u"Other"_s, metadata[3].album);

}

TEST(GstEnginePipelineTest, StreamStartResetsMetadataOnOrdinaryAndGaplessPaths) {

  GstStartup::Initialize();

  const QUrl current_media_url(u"https://example.test/current-media"_s);
  const QUrl current_stream_url(u"https://example.test/current-stream"_s);
  QList<EngineMetadata> ordinary_metadata;
  GstEnginePipeline ordinary_pipeline;
  GstEnginePipelineTest::SetCurrentUrls(ordinary_pipeline, current_media_url, current_stream_url);
  GstEnginePipelineTest::CaptureMetadata(ordinary_pipeline, ordinary_metadata);

  GstEnginePipelineTest::DispatchTags(ordinary_pipeline, {{GST_TAG_TITLE, uR"({"title":"Old title","artist":"JSON old artist","album":"JSON old album"})"_s}, {GST_TAG_ARTIST, u"Explicit old artist"_s}, {GST_TAG_ALBUM, u"Explicit old album"_s}, {GST_TAG_COMMENT, u"Old comment"_s}, {GST_TAG_LYRICS, u"Old lyrics"_s}}, 256000);
  // This is the ordinary first STREAM_START path: there is no initialized GstPipeline and no next URI to promote.
  GstEnginePipelineTest::DispatchStreamStart(ordinary_pipeline);
  GstEnginePipelineTest::DispatchTags(ordinary_pipeline, {{GST_TAG_TITLE, uR"({"title":"New title","artist":"New artist","album":"New album"})"_s}});

  ASSERT_EQ(2, ordinary_metadata.size());
  EXPECT_EQ(u"New title"_s, ordinary_metadata[1].title);
  EXPECT_EQ(u"New artist"_s, ordinary_metadata[1].artist);
  EXPECT_EQ(u"New album"_s, ordinary_metadata[1].album);
  EXPECT_TRUE(ordinary_metadata[1].comment.isEmpty());
  EXPECT_TRUE(ordinary_metadata[1].lyrics.isEmpty());
  EXPECT_EQ(0, ordinary_metadata[1].bitrate);
  EXPECT_EQ(EngineMetadata::Type::Current, ordinary_metadata[1].type);
  EXPECT_EQ(current_media_url, ordinary_metadata[1].media_url);
  EXPECT_EQ(current_stream_url, ordinary_metadata[1].stream_url);

  const QUrl next_media_url(u"https://example.test/next-media"_s);
  const QUrl next_stream_url(u"https://example.test/next-stream"_s);
  QList<EngineMetadata> gapless_metadata;
  GstEnginePipeline gapless_pipeline;
  GstEnginePipelineTest::SetCurrentUrls(gapless_pipeline, current_media_url, current_stream_url);
  GstEnginePipelineTest::CaptureMetadata(gapless_pipeline, gapless_metadata);

  GstEnginePipelineTest::DispatchTags(gapless_pipeline, {{GST_TAG_TITLE, uR"({"title":"Old title","artist":"JSON old artist","album":"JSON old album"})"_s}, {GST_TAG_ARTIST, u"Explicit old artist"_s}, {GST_TAG_ALBUM, u"Explicit old album"_s}, {GST_TAG_COMMENT, u"Old comment"_s}, {GST_TAG_LYRICS, u"Old lyrics"_s}}, 256000);
  GstEnginePipelineTest::SetNextUrlsAndMarkSet(gapless_pipeline, next_media_url, next_stream_url);
  GstEnginePipelineTest::DispatchStreamStart(gapless_pipeline);
  GstEnginePipelineTest::DispatchTags(gapless_pipeline, {{GST_TAG_TITLE, uR"({"title":"Gapless title","artist":"Gapless artist","album":"Gapless album"})"_s}});

  ASSERT_EQ(2, gapless_metadata.size());
  EXPECT_EQ(u"Gapless title"_s, gapless_metadata[1].title);
  EXPECT_EQ(u"Gapless artist"_s, gapless_metadata[1].artist);
  EXPECT_EQ(u"Gapless album"_s, gapless_metadata[1].album);
  EXPECT_TRUE(gapless_metadata[1].comment.isEmpty());
  EXPECT_TRUE(gapless_metadata[1].lyrics.isEmpty());
  EXPECT_EQ(0, gapless_metadata[1].bitrate);
  EXPECT_EQ(EngineMetadata::Type::Current, gapless_metadata[1].type);
  EXPECT_EQ(next_media_url, gapless_metadata[1].media_url);
  EXPECT_EQ(next_stream_url, gapless_metadata[1].stream_url);

}

TEST(GstEnginePipelineTest, InitFromUrlResetsRetainedMetadata) {

  GstStartup::Initialize();

  QList<EngineMetadata> metadata;
  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  GstEnginePipelineTest::DispatchTags(pipeline, {{GST_TAG_TITLE, uR"({"title":"Old title","artist":"JSON old artist","album":"JSON old album"})"_s}, {GST_TAG_ARTIST, u"Explicit old artist"_s}, {GST_TAG_ALBUM, u"Explicit old album"_s}, {GST_TAG_COMMENT, u"Old comment"_s}, {GST_TAG_LYRICS, u"Old lyrics"_s}}, 256000);

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();
  GstEnginePipelineTest::CaptureMetadata(pipeline, metadata);

  GstEnginePipelineTest::DispatchTags(pipeline, {{GST_TAG_TITLE, uR"({"title":"New title","artist":"New artist","album":"New album"})"_s}});

  ASSERT_EQ(1, metadata.size());
  EXPECT_EQ(u"New title"_s, metadata[0].title);
  EXPECT_EQ(u"New artist"_s, metadata[0].artist);
  EXPECT_EQ(u"New album"_s, metadata[0].album);
  EXPECT_TRUE(metadata[0].comment.isEmpty());
  EXPECT_TRUE(metadata[0].lyrics.isEmpty());
  EXPECT_EQ(0, metadata[0].bitrate);
  EXPECT_EQ(EngineMetadata::Type::Current, metadata[0].type);
  EXPECT_EQ(url, metadata[0].media_url);
  EXPECT_EQ(url, metadata[0].stream_url);

}

TEST(GstEnginePipelineTest, WhitespaceTagsClearValuesButAbsentTagsRetainThem) {

  GstStartup::Initialize();

  QList<EngineMetadata> metadata;
  GstEnginePipeline pipeline;
  GstEnginePipelineTest::CaptureMetadata(pipeline, metadata);

  GstEnginePipelineTest::DispatchTags(pipeline, {{GST_TAG_TITLE, uR"({"title":"JSON title","artist":"JSON artist","album":"JSON album"})"_s}, {GST_TAG_ARTIST, u"Explicit artist"_s}, {GST_TAG_ALBUM, u"Explicit album"_s}});
  // GStreamer omits zero-length string tags, so whitespace represents present values that ParseStrTag trims to empty.
  GstEnginePipelineTest::DispatchTags(pipeline, {{GST_TAG_ARTIST, u" "_s}, {GST_TAG_ALBUM, u" \t "_s}});
  GstEnginePipelineTest::DispatchTags(pipeline);

  ASSERT_EQ(3, metadata.size());
  EXPECT_EQ(u"Explicit artist"_s, metadata[0].artist);
  EXPECT_EQ(u"Explicit album"_s, metadata[0].album);
  EXPECT_EQ(u"JSON title"_s, metadata[1].title);
  EXPECT_EQ(u"JSON artist"_s, metadata[1].artist);
  EXPECT_EQ(u"JSON album"_s, metadata[1].album);
  EXPECT_EQ(metadata[1].title, metadata[2].title);
  EXPECT_EQ(metadata[1].artist, metadata[2].artist);
  EXPECT_EQ(metadata[1].album, metadata[2].album);
  EXPECT_EQ(EngineMetadata::Type::Current, metadata[2].type);

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

// Regression test: a buffering message from the queue while waiting for the warm-up made playback not pending, although the warm-up still starts playback, so pausing then was ignored, this made CancelledWarmupIsNotPending fail when the buffering finished after the preroll.
TEST(GstEnginePipelineTest, WarmupIsPendingWhileBuffering) {

  GstStartup::Initialize();

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  GstEnginePipeline pipeline;
  pipeline.set_output_device(u"fakesink"_s, QVariant());
  pipeline.set_device_warmup_duration_ms(2000);
  QString error;
  ASSERT_TRUE(pipeline.InitFromUrl(url, url, url.toEncoded(), 0, 0, 0.0, error)) << error.toStdString();

  // Wait until the pipeline is prerolled, waiting for the warm-up, and no longer buffering.
  pipeline.Play(false, 0);
  ASSERT_TRUE(WaitFor([&pipeline]() { return pipeline.state() == GST_STATE_PAUSED && pipeline.play_pending(); }));

  // Buffering while waiting for the warm-up, playback is still pending, the events are not processed in between, so no buffering message changes it.
  GstEnginePipelineTest::SetBuffering(pipeline, true);
  EXPECT_TRUE(pipeline.play_pending());

  // The warm-up starts playback.
  GstEnginePipelineTest::SetBuffering(pipeline, false);
  GstEnginePipelineTest::DeviceWarmupElapsed(pipeline);
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
