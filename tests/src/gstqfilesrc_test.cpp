// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "gtest_include.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/pbutils/pbutils.h>

#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

#include "engine/gstqfilesrc.h"
#include "engine/gststartup.h"
#include "tagreader/taglibfilestream_android.h"
#include "test_utils.h"

#include <taglib/fileref.h>
#include <memory>

namespace {

TEST(GstQFileSourceTest, ReadsAndSeeksUsingFileUri) {
  GstStartup::Initialize();
  ASSERT_TRUE(RegisterGstQFileSource());
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("space % #\u97f3.wv"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  QByteArray bytes(16384, '\0');
  for (qsizetype i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<char>(i / 1024);
  ASSERT_EQ(file.write(bytes), bytes.size());
  file.close();

  GError *error = nullptr;
  GstElement *source = gst_element_make_from_uri(GST_URI_SRC, QUrl::fromLocalFile(path).toEncoded().constData(), nullptr, &error);
  ASSERT_NE(source, nullptr);
  EXPECT_EQ(error, nullptr);
  EXPECT_STREQ(G_OBJECT_TYPE_NAME(source), "GstQFileSource");
  GstElement *pipeline = gst_pipeline_new(nullptr);
  GstElement *sink = gst_element_factory_make("appsink", nullptr);
  ASSERT_NE(sink, nullptr);
  g_object_set(sink, "sync", FALSE, nullptr);
  gst_bin_add_many(GST_BIN(pipeline), source, sink, nullptr);
  ASSERT_TRUE(gst_element_link(source, sink));
  EXPECT_NE(gst_element_set_state(pipeline, GST_STATE_PAUSED), GST_STATE_CHANGE_FAILURE);
  GstSample *sample = gst_app_sink_try_pull_preroll(GST_APP_SINK(sink), 5 * GST_SECOND);
  EXPECT_NE(sample, nullptr);
  if (sample) {
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    EXPECT_GT(gst_buffer_get_size(buffer), 0);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      EXPECT_EQ(QByteArray(reinterpret_cast<const char*>(map.data), static_cast<qsizetype>(map.size)), bytes.left(static_cast<qsizetype>(map.size)));
      gst_buffer_unmap(buffer, &map);
    }
    else {
      ADD_FAILURE() << "Could not map initial audio bytes";
    }
    gst_sample_unref(sample);
  }
  EXPECT_TRUE(gst_element_seek_simple(pipeline, GST_FORMAT_BYTES, GST_SEEK_FLAG_FLUSH, 8192));
  sample = gst_app_sink_try_pull_preroll(GST_APP_SINK(sink), 5 * GST_SECOND);
  EXPECT_NE(sample, nullptr);
  if (sample) {
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    EXPECT_EQ(GST_BUFFER_OFFSET(buffer), 8192);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      EXPECT_EQ(QByteArray(reinterpret_cast<const char*>(map.data), static_cast<qsizetype>(map.size)), bytes.mid(8192, static_cast<qsizetype>(map.size)));
      gst_buffer_unmap(buffer, &map);
    }
    else {
      ADD_FAILURE() << "Could not map seeked audio bytes";
    }
    gst_sample_unref(sample);
  }
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}

TEST(GstQFileSourceTest, MissingFileFailsExplicitly) {
  GstStartup::Initialize();
  ASSERT_TRUE(RegisterGstQFileSource());
  QTemporaryDir directory;
  GstElement *source = gst_element_make_from_uri(GST_URI_SRC, QUrl::fromLocalFile(directory.filePath(QStringLiteral("absent.wv"))).toEncoded().constData(), nullptr, nullptr);
  ASSERT_NE(source, nullptr);
  EXPECT_EQ(gst_element_set_state(source, GST_STATE_PAUSED), GST_STATE_CHANGE_FAILURE);
  gst_element_set_state(source, GST_STATE_NULL);
  gst_object_unref(source);
}

TEST(GstQFileSourceTest, DecodesAndDiscoversWavAndPreservesTagLibFilename) {
  GstStartup::Initialize();
  ASSERT_TRUE(RegisterGstQFileSource());
  TemporaryResource resource(QStringLiteral(":/audio/strawberry.wav"));
  ASSERT_TRUE(resource.open());
  resource.close();
  GError *error = nullptr;
  GstDiscoverer *discoverer = gst_discoverer_new(5 * GST_SECOND, &error);
  ASSERT_NE(discoverer, nullptr);
  GstDiscovererInfo *info = gst_discoverer_discover_uri(discoverer, QUrl::fromLocalFile(resource.fileName()).toEncoded().constData(), &error);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(gst_discoverer_info_get_result(info), GST_DISCOVERER_OK);
  EXPECT_GT(gst_discoverer_info_get_duration(info), 0);
  gst_discoverer_info_unref(info);
  gst_object_unref(discoverer);
  EXPECT_EQ(error, nullptr);

  std::unique_ptr<TagLib::IOStream> stream(CreateAndroidTagLibStream(resource.fileName()));
  ASSERT_TRUE(stream->isOpen());
  EXPECT_STREQ(stream->name(), QFile::encodeName(resource.fileName()).constData());
  TagLib::FileRef ref(stream.get());
  EXPECT_FALSE(ref.isNull());
  ASSERT_NE(ref.audioProperties(), nullptr);
  EXPECT_GT(ref.audioProperties()->sampleRate(), 0);
}

}  // namespace
