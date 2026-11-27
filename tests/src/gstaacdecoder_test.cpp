// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <QByteArray>

#include "engine/gstaacdecoder.h"
#include "engine/gststartup.h"

namespace {

QByteArray EncodeAac(bool adts, GstCaps **caps) {
  GError *error = nullptr;
  const char *description = adts ?
      "audiotestsrc num-buffers=12 ! audioconvert ! avenc_aac ! aacparse ! audio/mpeg,stream-format=adts ! appsink name=out sync=false" :
      "audiotestsrc num-buffers=12 ! audioconvert ! avenc_aac ! aacparse ! audio/mpeg,stream-format=raw ! appsink name=out sync=false";
  GstElement *pipeline = gst_parse_launch(description, &error);
  if (error) {
    ADD_FAILURE() << error->message;
    g_error_free(error);
    if (pipeline) gst_object_unref(pipeline);
    return {};
  }
  GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "out");
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  QByteArray result;
  while (GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 5 * GST_SECOND)) {
    gst_caps_replace(caps, gst_sample_get_caps(sample));
    GstMapInfo map;
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      if (adts || result.isEmpty()) result.append(reinterpret_cast<const char*>(map.data), static_cast<qsizetype>(map.size));
      gst_buffer_unmap(buffer, &map);
    }
    gst_sample_unref(sample);
  }
  EXPECT_TRUE(gst_app_sink_is_eos(GST_APP_SINK(sink)));
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(sink);
  gst_object_unref(pipeline);
  return result;
}

class AacDecoderTest : public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    GstStartup::Initialize();
    for (const char *name : {"avenc_aac", "avdec_aac"}) {
      GstElementFactory *factory = gst_element_factory_find(name);
      if (!factory) GTEST_SKIP() << "Optional native GStreamer libav plugin is required: " << name;
      gst_object_unref(factory);
    }
    ASSERT_TRUE(RegisterGstAacDecoder("avdec_aac"));
  }
};

struct DecoderInput {
  bool wrapper_used = false;
  bool raw_caps = false;
  QByteArray payload;
};

TEST_P(AacDecoderTest, DecodesThroughExpectedFactoryWithRawCodecDataAndUnchangedPayload) {
  const bool adts = GetParam() != 0;
  GstCaps *caps = nullptr;
  QByteArray bytes = EncodeAac(adts, &caps);
  QByteArray expected_payload;
  ASSERT_FALSE(bytes.isEmpty());
  for (qsizetype offset = 0; adts && offset < bytes.size();) {
    ASSERT_GE(bytes.size() - offset, 7);
    auto *header = reinterpret_cast<unsigned char*>(bytes.data() + offset);
    ASSERT_EQ(header[0], 0xff);
    const int size = ((header[3] & 3) << 11) | (header[4] << 3) | (header[5] >> 5);
    ASSERT_GE(size, 7);
    ASSERT_LE(offset + size, bytes.size());
    expected_payload.append(bytes.constData() + offset + 7, size - 7);
    header[1] = static_cast<unsigned char>(GetParam() == 2 ? header[1] | 8 : header[1] & ~8);
    offset += size;
  }
  if (!adts) expected_payload = bytes;
  GstElement *pipeline = gst_parse_launch("appsrc name=in format=time ! decodebin3 ! appsink name=out sync=false", nullptr);
  ASSERT_NE(pipeline, nullptr);
  DecoderInput observed;
  g_signal_connect(pipeline, "deep-element-added", G_CALLBACK(+[](GstBin *, GstBin *, GstElement *element, gpointer data) {
    auto *state = static_cast<DecoderInput*>(data);
    GstElementFactory *factory = gst_element_get_factory(element);
    if (!factory) return;
    const char *name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));
    if (g_strcmp0(name, "strawberryadtsdecoder") == 0) state->wrapper_used = true;
    if (g_strcmp0(name, "avdec_aac") != 0) return;
    GstPad *pad = gst_element_get_static_pad(element, "sink");
    gst_pad_add_probe(pad, static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_BUFFER), +[](GstPad *, GstPadProbeInfo *info, gpointer probe_data) {
      auto *input_state = static_cast<DecoderInput*>(probe_data);
      if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
        GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
        if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
          GstCaps *input_caps = nullptr;
          gst_event_parse_caps(event, &input_caps);
          const GstStructure *structure = gst_caps_get_structure(input_caps, 0);
          int version = 0;
          input_state->raw_caps = gst_structure_get_int(structure, "mpegversion", &version) && version == 4 &&
                               g_strcmp0(gst_structure_get_string(structure, "stream-format"), "raw") == 0 &&
                               gst_structure_has_field_typed(structure, "codec_data", GST_TYPE_BUFFER);
        }
      }
      else {
        GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
          input_state->payload.append(reinterpret_cast<const char*>(map.data), static_cast<qsizetype>(map.size));
          gst_buffer_unmap(buffer, &map);
        }
      }
      return GST_PAD_PROBE_OK;
    }, state, nullptr);
    gst_object_unref(pad);
  }), &observed);
  GstElement *input = gst_bin_get_by_name(GST_BIN(pipeline), "in");
  GstElement *output = gst_bin_get_by_name(GST_BIN(pipeline), "out");
  gst_app_src_set_caps(GST_APP_SRC(input), caps);
  gst_caps_unref(caps);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  GstBuffer *buffer = gst_buffer_new_allocate(nullptr, static_cast<gsize>(bytes.size()), nullptr);
  gst_buffer_fill(buffer, 0, bytes.constData(), static_cast<gsize>(bytes.size()));
  EXPECT_EQ(gst_app_src_push_buffer(GST_APP_SRC(input), buffer), GST_FLOW_OK);
  gst_app_src_end_of_stream(GST_APP_SRC(input));
  int samples = 0;
  while (GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(output), 5 * GST_SECOND)) {
    EXPECT_GT(gst_buffer_get_size(gst_sample_get_buffer(sample)), 0);
    ++samples;
    gst_sample_unref(sample);
  }
  EXPECT_GT(samples, 0);
  EXPECT_TRUE(gst_app_sink_is_eos(GST_APP_SINK(output)));
  gst_element_set_state(pipeline, GST_STATE_NULL);
  EXPECT_EQ(observed.wrapper_used, adts);
  EXPECT_TRUE(observed.raw_caps);
  EXPECT_EQ(observed.payload, expected_payload);
  gst_object_unref(input);
  gst_object_unref(output);
  gst_object_unref(pipeline);
}

INSTANTIATE_TEST_SUITE_P(RawAndAdtsVersions, AacDecoderTest, ::testing::Values(0, 2, 4));

}  // namespace
