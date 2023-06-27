/*
 * Strawberry Music Player
 * Copyright 2023 Roman Lebedev
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

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>

#include <ebur128.h>
#include <glib-object.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QString>
#include <QThread>
#include <QtGlobal>

#include "core/logging.h"
#include "core/signalchecker.h"

#include "ebur128analysis.h"

static const int kTimeoutSecs = 60;

namespace {

struct ebur128_state_deleter {
  void operator()(ebur128_state *p) const { ebur128_destroy(&p); };
};

struct GstSampleDeleter {
  void operator()(GstSample *s) const { gst_sample_unref(s); };
};

struct FrameFormat {
  enum class DataFormat {
    S16,
    S32,
    FP32,
    FP64
  };

  int channels;
  int samplerate;
  DataFormat format;

  explicit FrameFormat(GstCaps *caps);
};

class EBUR128State {
 public:
  EBUR128State() = delete;
  EBUR128State(const EBUR128State&) = delete;
  EBUR128State(EBUR128State&&) = delete;

  EBUR128State &operator=(const EBUR128State&) = delete;
  EBUR128State &operator=(EBUR128State&&) = delete;

  explicit EBUR128State(FrameFormat dsc_);
  const FrameFormat dsc;

  void AddFrames(const char *data, size_t size);

  static std::optional<EBUR128Measures> Finalize(EBUR128State &&state);

 private:
  std::unique_ptr<ebur128_state, ebur128_state_deleter> st;
};

class EBUR128AnalysisImpl {
  EBUR128AnalysisImpl() = default;

 public:
  static std::optional<EBUR128Measures> Compute(const Song &song);

 private:
  GstElement *convert_element_ = nullptr;

  std::optional<EBUR128State> state;

  static void NewPadCallback(GstElement *elt, GstPad *pad, gpointer data);
  static GstFlowReturn NewBufferCallback(GstAppSink *app_sink, gpointer self);
};

FrameFormat::FrameFormat(GstCaps *caps) {

  GstStructure *structure = gst_caps_get_structure(caps, 0);
  QString format_str = gst_structure_get_string(structure, "format");
  gst_structure_get_int(structure, "rate", &samplerate);
  gst_structure_get_int(structure, "channels", &channels);

  if (format_str == "S16LE") {
    format = DataFormat::S16;
  }
  else if (format_str == "S32LE") {
    format = DataFormat::S32;
  }
  else if (format_str == "F32LE") {
    format = DataFormat::FP32;
  }
  else if (format_str == "F64LE") {
    format = DataFormat::FP64;
  }
  else {
    qLog(Error) << "EBUR128AnalysisImpl: got unexpected format " << format_str;
    Q_ASSERT(false && "Unexpected format. How did you get here?");
  }

}

bool operator==(const FrameFormat &lhs, const FrameFormat &rhs) {

  return std::tie(lhs.channels, lhs.samplerate, lhs.format) == std::tie(rhs.channels, rhs.samplerate, rhs.format);

}
bool operator!=(const FrameFormat &lhs, const FrameFormat &rhs) {

  return !(lhs == rhs);

}

EBUR128State::EBUR128State(FrameFormat dsc_) : dsc(dsc_) {

  st.reset(ebur128_init(dsc.channels, dsc.samplerate, EBUR128_MODE_I | EBUR128_MODE_LRA));
  Q_ASSERT(st);

};

void EBUR128State::AddFrames(const char *data, size_t size) {

  Q_ASSERT(st);

  int bytes_per_sample = -1;
  switch (dsc.format) {
    case FrameFormat::DataFormat::S16:
      bytes_per_sample = sizeof(int16_t);
      break;
    case FrameFormat::DataFormat::S32:
      bytes_per_sample = sizeof(int32_t);
      break;
    case FrameFormat::DataFormat::FP32:
      bytes_per_sample = sizeof(float);
      break;
    case FrameFormat::DataFormat::FP64:
      bytes_per_sample = sizeof(double);
      break;
  }

  int bytes_per_frame = dsc.channels * bytes_per_sample;
  Q_ASSERT(size % bytes_per_frame == 0);
  auto num_frames = size / bytes_per_frame;

  int ebur_error;
  switch (dsc.format) {
    case FrameFormat::DataFormat::S16:
      ebur_error = ebur128_add_frames_short(&*st, reinterpret_cast<const int16_t*>(data), num_frames);
      break;
    case FrameFormat::DataFormat::S32:
      ebur_error = ebur128_add_frames_int(&*st, reinterpret_cast<const int32_t*>(data), num_frames);
      break;
    case FrameFormat::DataFormat::FP32:
      ebur_error = ebur128_add_frames_float(&*st, reinterpret_cast<const float*>(data), num_frames);
      break;
    case FrameFormat::DataFormat::FP64:
      ebur_error = ebur128_add_frames_double(&*st, reinterpret_cast<const double*>(data), num_frames);
      break;
  }
  Q_ASSERT(ebur_error == EBUR128_SUCCESS);

}

std::optional<EBUR128Measures> EBUR128State::Finalize(EBUR128State&& state)  {

  ebur128_state *ebur128 = &*state.st;

  EBUR128Measures result;

  double out = NAN;
  int ebur_error = ebur128_loudness_global(ebur128, &out);
  Q_ASSERT(ebur_error == EBUR128_SUCCESS);
  result.loudness_lufs = out;

  out = NAN;
  ebur_error = ebur128_loudness_range(ebur128, &out);
  Q_ASSERT(ebur_error == EBUR128_SUCCESS);
  result.range_lu = out;

  return result;

}

void EBUR128AnalysisImpl::NewPadCallback(GstElement *elt, GstPad *pad, gpointer data) {

  Q_UNUSED(elt);

  EBUR128AnalysisImpl *me = reinterpret_cast<EBUR128AnalysisImpl*>(data);
  GstPad *const audiopad = gst_element_get_static_pad(me->convert_element_, "sink");

  if (GST_PAD_IS_LINKED(audiopad)) {
    qLog(Warning) << "audiopad is already linked, unlinking old pad";
    gst_pad_unlink(audiopad, GST_PAD_PEER(audiopad));
  }

  gst_pad_link(pad, audiopad);
  gst_object_unref(audiopad);

}

GstFlowReturn EBUR128AnalysisImpl::NewBufferCallback(GstAppSink *app_sink, gpointer self) {

  EBUR128AnalysisImpl *me = reinterpret_cast<EBUR128AnalysisImpl*>(self);

  std::unique_ptr<GstSample, GstSampleDeleter> sample(gst_app_sink_pull_sample(app_sink));
  if (!sample) return GST_FLOW_ERROR;

  const FrameFormat dsc(gst_sample_get_caps(&*sample));
  if (!me->state) {
    me->state.emplace(dsc);
  }
  else if (me->state->dsc != dsc) {
    return GST_FLOW_ERROR;
  }

  GstBuffer *buffer = gst_sample_get_buffer(&*sample);
  if (buffer) {
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      me->state->AddFrames(reinterpret_cast<const char*>(map.data), static_cast<qint64>(map.size));
      gst_buffer_unmap(buffer, &map);
    }
  }

  return GST_FLOW_OK;

}

GstElement *CreateElement(const QString &factory_name, GstElement *bin) {

  GstElement *ret = gst_element_factory_make(factory_name.toLatin1().constData(), factory_name.toLatin1().constData());

  if (ret && bin) gst_bin_add(GST_BIN(bin), ret);

  if (!ret) {
    qLog(Warning) << "Couldn't create the gstreamer element" << factory_name;
  }

  return ret;

}

std::optional<EBUR128Measures> EBUR128AnalysisImpl::Compute(const Song &song) {

  EBUR128AnalysisImpl impl;

  GstElement *pipeline = gst_pipeline_new("pipeline");
  if (!pipeline) {
    return std::nullopt;
  }

  GstElement *src = CreateElement("filesrc", pipeline);
  GstElement *decode = CreateElement("decodebin", pipeline);
  GstElement *convert = CreateElement("audioconvert", pipeline);
  GstElement *sink = CreateElement("appsink", pipeline);

  if (!src || !decode || !convert || !sink) {
    gst_object_unref(pipeline);
    return std::nullopt;
  }

  impl.convert_element_ = convert;

  // Connect the elements
  gst_element_link_many(src, decode, nullptr);

  GstStaticCaps static_caps = GST_STATIC_CAPS(
    "audio/x-raw,"
    "format = (string) { S16LE, S32LE, F32LE, F64LE },"
    "layout = (string) interleaved");

  GstCaps *caps = gst_static_caps_get(&static_caps);
  gst_element_link_filtered(convert, sink, caps);
  gst_caps_unref(caps);

  GstAppSinkCallbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.new_sample = NewBufferCallback;
  gst_app_sink_set_callbacks(reinterpret_cast<GstAppSink*>(sink), &callbacks, &impl, nullptr);
  g_object_set(G_OBJECT(sink), "sync", FALSE, nullptr);
  g_object_set(G_OBJECT(sink), "emit-signals", TRUE, nullptr);

  // Set the filename
  g_object_set(src, "location", song.url().toLocalFile().toUtf8().constData(), nullptr);

  // Connect signals
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  CHECKED_GCONNECT(decode, "pad-added", &NewPadCallback, &impl);

  // Play only the specified song!
  gst_element_set_state(pipeline, GST_STATE_PAUSED);
  // wait for state change before seeking
  gst_element_get_state(pipeline, nullptr, nullptr, kTimeoutSecs * GST_SECOND);
  gst_element_seek(pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, song.beginning_nanosec() * GST_NSECOND, GST_SEEK_TYPE_SET, song.end_nanosec() * GST_NSECOND);

  QElapsedTimer time;
  time.start();

  // Start playing
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  // Wait until EOS or error
  bool hadError = false;
  GstMessage *msg = gst_bus_timed_pop_filtered(bus, kTimeoutSecs * GST_SECOND, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
  if (msg) {
    if (msg->type == GST_MESSAGE_ERROR) {
      hadError = true;
      // Report error
      GError *error = nullptr;
      gchar *debugs = nullptr;
      gst_message_parse_error(msg, &error, &debugs);
      if (error) {
        QString message = QString::fromLocal8Bit(error->message);
        g_error_free(error);
        qLog(Debug) << "Error processing " << song.url() << ":" << message;
      }
      if (debugs) free(debugs);
    }
    gst_message_unref(msg);
  }

  const qint64 decode_time = time.restart();

  std::optional<EBUR128Measures> result;
  if (!hadError && impl.state) {
    // Generate loudness characteristics from sampled data.
    result = EBUR128State::Finalize(std::move(impl.state.value()));

    const qint64 finalize_time = time.elapsed();

    qLog(Debug) << "Decode time:" << decode_time << "Finalization time:" << finalize_time;
  }

  // Cleanup
  callbacks.new_sample = nullptr;
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);

  return result;

}

};  // namespace

std::optional<EBUR128Measures> EBUR128Analysis::Compute(const Song &song) {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  return EBUR128AnalysisImpl::Compute(song);

}
