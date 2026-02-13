/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <glib-object.h>
#include <cstdlib>
#include <cstring>
#include <chromaprint.h>
#include <gst/gst.h>

#include <QtGlobal>
#include <QCoreApplication>
#include <QApplication>
#include <QThread>
#include <QIODevice>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QMutexLocker>
#include <QElapsedTimer>

#include "chromaprinter.h"
#include "core/logging.h"
#include "core/signalchecker.h"

using namespace Qt::Literals::StringLiterals;

#ifndef u_int32_t
using u_int32_t = unsigned int;
#endif

namespace {
constexpr int kTimeoutSecs = 30;
constexpr int kMinimumEncodedFingerprintLength = 16;
constexpr int kMaxPcmSeconds = 120;

// Parameters for CreateFingerprint()'s legacy algorithm, kept separate from the hardened CreateFullFingerprint() pipeline above so its output never drifts from what was already stored for existing libraries.
constexpr int kLegacyDecodeRate = 11025;
constexpr int kLegacyDecodeChannels = 1;
constexpr int kLegacyPlayLengthSecs = 30;
constexpr int kLegacyTimeoutSecs = 10;
}  // namespace

Chromaprinter::Chromaprinter(const QUrl &url)
    : url_(url),
      sample_rate_(0),
      channels_(0),
      max_pcm_bytes_(0),
      pcm_limit_reached_(false),
      convert_element_(nullptr) {}

Chromaprinter::Chromaprinter(const QString &filename)
    : url_(QUrl::fromLocalFile(filename)),
      sample_rate_(0),
      channels_(0),
      max_pcm_bytes_(0),
      pcm_limit_reached_(false),
      convert_element_(nullptr) {}

QString Chromaprinter::LastError() const {
  const QMutexLocker locker(&mutex_last_error_);
  return last_error_;
}

void Chromaprinter::ClearLastError() {
  const QMutexLocker locker(&mutex_last_error_);
  last_error_.clear();
}

void Chromaprinter::SetLastError(const QString &error) {
  const QMutexLocker locker(&mutex_last_error_);
  last_error_ = error;
}

void Chromaprinter::SetLastErrorIfEmpty(const QString &error) {
  const QMutexLocker locker(&mutex_last_error_);
  if (last_error_.isEmpty()) {
    last_error_ = error;
  }
}

QString Chromaprinter::TrimmedLastError() const {
  const QMutexLocker locker(&mutex_last_error_);
  return last_error_.trimmed();
}

QByteArray Chromaprinter::ToGstUrl(const QUrl &url) {

  if (url.isLocalFile() && !url.host().isEmpty()) {
    const QString str = "file:////"_L1 + url.host() + url.path();
    return str.toUtf8();
  }

  return url.toEncoded();

}

GstElement *Chromaprinter::CreateElement(const QString &factory_name, GstElement *bin) {

  GstElement *element = gst_element_factory_make(factory_name.toLatin1().constData(), factory_name.toLatin1().constData());

  if (element && bin) gst_bin_add(GST_BIN(bin), element);

  if (!element) {
    qLog(Error) << "Couldn't create the gstreamer element" << factory_name;
  }

  return element;

}

QString Chromaprinter::CreateFingerprintInternal(const bool legacy) {

  Q_ASSERT(qobject_cast<QApplication*>(QCoreApplication::instance()) == nullptr || QThread::currentThread() != qApp->thread());

  sample_rate_ = 0;
  channels_ = 0;
  max_pcm_bytes_ = 0;
  pcm_limit_reached_ = false;

  ClearLastError();

  auto set_error = [this](const QString &error) {
    SetLastError(error);
    qLog(Warning) << "Chromaprinter:" << error << "File:" << url_;
  };

  if (!buffer_.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    set_error(u"Could not open temporary fingerprint buffer"_s);
    return QString();
  }

  GstElement *pipeline = gst_pipeline_new("pipeline");
  if (!pipeline) {
    buffer_.close();
    set_error(u"Could not create GStreamer pipeline"_s);
    return QString();
  }

  GstElement *decode = CreateElement(u"uridecodebin"_s, pipeline);
  GstElement *convert = CreateElement(u"audioconvert"_s, pipeline);
  // Legacy mode needs an explicit resample stage to force the fixed 11025Hz rate; the hardened path lets GStreamer keep whatever rate the source decodes to.
  GstElement *resample = legacy ? CreateElement(u"audioresample"_s, pipeline) : nullptr;
  GstElement *queue = CreateElement(u"queue2"_s, pipeline);
  GstElement *sink = CreateElement(u"appsink"_s, pipeline);
  if (!decode || !convert || !queue || !sink || (legacy && !resample)) {
    gst_object_unref(pipeline);
    buffer_.close();
    set_error(legacy ? u"Could not create one or more GStreamer elements (uridecodebin/audioconvert/audioresample/queue2/appsink)"_s
                     : u"Could not create one or more GStreamer elements (uridecodebin/audioconvert/queue2/appsink)"_s);
    return QString();
  }

  { const QMutexLocker locker(&mutex_state_); convert_element_ = convert; }

  // Connect the elements
  GstElement *pre_queue_element = convert;
  if (legacy) {
    if (!gst_element_link_many(convert, resample, nullptr)) {
      gst_object_unref(pipeline);
      buffer_.close();
      set_error(u"Failed to link audioconvert to audioresample"_s);
      return QString();
    }
    pre_queue_element = resample;
  }

  // Request 16-bit PCM. Legacy mode forces a fixed mono sample rate so its output keeps matching fingerprints already stored for existing libraries; otherwise GStreamer keeps whatever rate/channel count the source decodes to.
  GstCaps *caps = legacy
      ? gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING, "S16LE", "channels", G_TYPE_INT, kLegacyDecodeChannels, "rate", G_TYPE_INT, kLegacyDecodeRate, nullptr)
      : gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING, "S16LE", "layout", G_TYPE_STRING, "interleaved", "channels", GST_TYPE_INT_RANGE, 1, 2, nullptr);
  const gboolean queue_link_ok = gst_element_link_filtered(pre_queue_element, queue, caps);
  gst_caps_unref(caps);
  if (!queue_link_ok || !gst_element_link_many(queue, sink, nullptr)) {
    gst_object_unref(pipeline);
    buffer_.close();
    set_error(u"Failed to link audioconvert/queue2/appsink with required audio caps"_s);
    return QString();
  }

  GstAppSinkCallbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.new_sample = NewBufferCallback;
  gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);
  g_object_set(G_OBJECT(sink), "sync", FALSE, nullptr);
  g_object_set(G_OBJECT(sink), "emit-signals", TRUE, nullptr);

  // Use URI-based decodebin for more robust demux/decoder selection.
  const QByteArray gst_url = ToGstUrl(url_);
  g_object_set(G_OBJECT(decode), "uri", gst_url.constData(), nullptr);

  // Allow queueing of decoded audio to avoid starving appsink callback.
  // Keep the in-pipeline queue bounded as an additional memory guard.
  g_object_set(G_OBJECT(queue), "max-size-time", 10 * GST_SECOND, nullptr);
  g_object_set(G_OBJECT(queue), "max-size-buffers", 0, nullptr);
  g_object_set(G_OBJECT(queue), "max-size-bytes", 8 * 1024 * 1024, nullptr);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

  auto teardown_pipeline = [&]() {
    gst_object_unref(bus);
    // Wait for the streaming thread to actually stop before returning, so callers can safely touch state written by NewPadCallback/NewBufferCallback (e.g. buffer_) without holding mutex_state_.
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_element_get_state(pipeline, nullptr, nullptr, 5 * GST_SECOND);
    gst_object_unref(pipeline);
  };

  CHECKED_GCONNECT(decode, "pad-added", &NewPadCallback, this);

  const int timeout_secs = legacy ? kLegacyTimeoutSecs : kTimeoutSecs;

  QElapsedTimer time;
  time.start();

  if (legacy) {
    // Play only the first kLegacyPlayLengthSecs seconds, matching the window fingerprints already stored for existing libraries were generated from.
    if (gst_element_set_state(pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
      teardown_pipeline();
      buffer_.close();
      set_error(u"Failed to pause pipeline for legacy fingerprint window seek"_s);
      return QString();
    }
    // Wait for the state change before seeking.
    GstState state = GST_STATE_VOID_PENDING;
    if (gst_element_get_state(pipeline, &state, nullptr, timeout_secs * GST_SECOND) == GST_STATE_CHANGE_FAILURE || state != GST_STATE_PAUSED) {
      teardown_pipeline();
      buffer_.close();
      set_error(u"Pipeline did not reach paused state for legacy fingerprint window seek"_s);
      return QString();
    }
    if (!gst_element_seek(pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0 * GST_SECOND, GST_SEEK_TYPE_SET, kLegacyPlayLengthSecs * GST_SECOND)) {
      teardown_pipeline();
      buffer_.close();
      set_error(u"Failed to seek to legacy fingerprint window"_s);
      return QString();
    }
  }

  // Start playing
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  bool finished = false;
  QElapsedTimer bus_timer;
  bus_timer.start();

  while (!finished && bus_timer.elapsed() < timeout_secs * 1000) {
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, 200 * GST_MSECOND, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR | GST_MESSAGE_WARNING));
    if (!msg) continue;

    if (msg->type == GST_MESSAGE_ERROR) {
      GError *error = nullptr;
      gchar *debugs = nullptr;
      gst_message_parse_error(msg, &error, &debugs);
      if (error) {
        const QString message = QString::fromLocal8Bit(error->message);
        g_error_free(error);
        qLog(Debug) << "Error processing" << url_ << ":" << message;
        set_error(QStringLiteral("GStreamer decode error: %1").arg(message));
      }
      if (debugs) {
        g_free(debugs);
      }
      finished = true;
    }
    else if (msg->type == GST_MESSAGE_WARNING) {
      GError *warning = nullptr;
      gchar *debugs = nullptr;
      gst_message_parse_warning(msg, &warning, &debugs);
      if (warning) {
        const QString message = QString::fromLocal8Bit(warning->message);
        g_error_free(warning);
        qLog(Warning) << "Chromaprinter warning for" << url_ << ":" << message;
        SetLastErrorIfEmpty(QStringLiteral("GStreamer warning: %1").arg(message));
      }
      if (debugs) {
        g_free(debugs);
      }
    }
    else if (msg->type == GST_MESSAGE_EOS) {
      finished = true;
    }

    gst_message_unref(msg);
  }

  if (!finished) {
    qLog(Warning) << "Chromaprinter: Timed out waiting for EOS/error after" << timeout_secs << "seconds for" << url_;
  }

  // Stop the pipeline before closing/reading buffer_ to avoid concurrent writes from streaming threads.
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_element_get_state(pipeline, nullptr, nullptr, 5 * GST_SECOND);

  // Acquire mutex_state_ to ensure all prior callback writes (sample_rate_, channels_, max_pcm_bytes_, pcm_limit_reached_, buffer_) are visible on this thread.
  { const QMutexLocker locker(&mutex_state_); }

  const qint64 decode_time = time.restart();

  buffer_.close();

  // Generate fingerprint from recorded buffer data
  QByteArray data = buffer_.data();
  if (data.isEmpty()) {
    // Usually means decodebin never produced a usable raw audio pad.
    teardown_pipeline();
    QString error_message = u"No decoded audio samples were produced by GStreamer"_s;
    const QString callback_error = TrimmedLastError();
    if (!callback_error.isEmpty() && !error_message.contains(callback_error, Qt::CaseInsensitive)) {
      error_message += QStringLiteral(" %1").arg(callback_error);
    }
    set_error(error_message);
    return QString();
  }
  if (pcm_limit_reached_) {
    qLog(Debug) << "Chromaprinter: Reached in-memory PCM cap for" << url_ << "bytes stored" << data.size();
  }

  if (sample_rate_ <= 0 || channels_ <= 0) {
    teardown_pipeline();
    QString error_message = u"No valid PCM format metadata was received from GStreamer"_s;
    const QString callback_error = TrimmedLastError();
    if (!callback_error.isEmpty() && !error_message.contains(callback_error, Qt::CaseInsensitive)) {
      error_message += QStringLiteral(" %1").arg(callback_error);
    }
    set_error(error_message);
    return QString();
  }

  ChromaprintContext *chromaprint = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
  if (!chromaprint) {
    teardown_pipeline();
    set_error(u"Failed to create Chromaprint context"_s);
    return QString();
  }

  if (chromaprint_start(chromaprint, sample_rate_, channels_) != 1) {
    chromaprint_free(chromaprint);
    teardown_pipeline();
    set_error(QStringLiteral("Chromaprint could not start (sample rate %1, channels %2)").arg(sample_rate_).arg(channels_));
    return QString();
  }

  if (chromaprint_feed(chromaprint, reinterpret_cast<int16_t*>(data.data()), static_cast<int>(data.size() / 2)) != 1) {
    chromaprint_free(chromaprint);
    teardown_pipeline();
    set_error(u"Chromaprint failed while processing decoded samples"_s);
    return QString();
  }

  if (chromaprint_finish(chromaprint) != 1) {
    chromaprint_free(chromaprint);
    teardown_pipeline();
    set_error(u"Chromaprint could not finalize fingerprint"_s);
    return QString();
  }

  u_int32_t *fprint = nullptr;
  int size = 0;
  int result = chromaprint_get_raw_fingerprint(chromaprint, &fprint, &size);
  QByteArray fingerprint;
  if (result == 1) {
    char *encoded = nullptr;
    int encoded_size = 0;
    result = chromaprint_encode_fingerprint(fprint, size, CHROMAPRINT_ALGORITHM_DEFAULT, &encoded, &encoded_size, 1);
    if (result == 1) {
      fingerprint.append(encoded, encoded_size);
      chromaprint_dealloc(encoded);
    }
    else {
      set_error(u"Chromaprint failed to encode generated fingerprint"_s);
    }
    chromaprint_dealloc(fprint);
  }
  else {
    set_error(u"Chromaprint did not return a raw fingerprint"_s);
  }
  chromaprint_free(chromaprint);

  const qint64 codegen_time = time.elapsed();

  // Cleanup
  callbacks.new_sample = nullptr;
  gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);
  teardown_pipeline();

  const QString fingerprint_string = QString::fromUtf8(fingerprint);
  if (fingerprint_string.size() < kMinimumEncodedFingerprintLength) {
    set_error(QStringLiteral("Generated fingerprint is too short (%1 bytes)").arg(fingerprint_string.size()));
    return QString();
  }

  qLog(Debug) << "Chromaprinter: Generated fingerprint for" << url_ << "length:" << fingerprint_string.size() << "decode time:" << decode_time << "codegen time:" << codegen_time;

  return fingerprint_string;

}

QString Chromaprinter::CreateFingerprint() {
  return CreateFingerprintInternal(true);
}

QString Chromaprinter::CreateFullFingerprint() {
  return CreateFingerprintInternal(false);
}

void Chromaprinter::NewPadCallback(GstElement *element, GstPad *pad, gpointer data) {

  Q_UNUSED(element)

  Chromaprinter *instance = reinterpret_cast<Chromaprinter*>(data);
  if (!instance) {
    return;
  }
  GstElement *convert_element = nullptr;
  {
    const QMutexLocker locker(&instance->mutex_state_);
    convert_element = instance->convert_element_;
  }
  if (!convert_element) {
    return;
  }
  GstCaps *caps = gst_pad_get_current_caps(pad);
  if (!caps) {
    caps = gst_pad_query_caps(pad, nullptr);
  }
  QString caps_name;
  if (caps) {
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (structure) {
      const gchar *name = gst_structure_get_name(structure);
      if (name) {
        caps_name = QString::fromLatin1(name);
      }
    }
    gst_caps_unref(caps);
  }

  // uridecodebin can expose non-audio pads (video, subtitles) alongside the audio pad; ignore those so a failed link attempt on them does not clobber last_error_ ahead of the audio pad linking successfully.
  if (!caps_name.startsWith("audio/"_L1)) {
    return;
  }

  GstPad *const audiopad = gst_element_get_static_pad(convert_element, "sink");
  if (!audiopad) {
    instance->SetLastErrorIfEmpty(u"Could not obtain audioconvert sink pad"_s);
    return;
  }

  if (GST_PAD_IS_LINKED(audiopad)) {
    gst_object_unref(audiopad);
    return;
  }

  const GstPadLinkReturn pad_link_return = gst_pad_link(pad, audiopad);
  if (pad_link_return != GST_PAD_LINK_OK) {
    const QString reason = QStringLiteral("Failed to link decodebin pad '%1' to audioconvert (code %2)").arg(caps_name).arg(static_cast<int>(pad_link_return));
    instance->SetLastErrorIfEmpty(reason);
    qLog(Warning) << "Chromaprinter:" << reason << "File:" << instance->url_;
  }
  gst_object_unref(audiopad);

}

GstFlowReturn Chromaprinter::NewBufferCallback(GstAppSink *app_sink, gpointer self) {

  Chromaprinter *instance = reinterpret_cast<Chromaprinter*>(self);

  GstSample *sample = gst_app_sink_pull_sample(app_sink);
  if (!sample) return GST_FLOW_ERROR;

  // Extract format metadata from caps before locking, as GstCaps/GstStructure are borrowed refs owned by the sample.
  int sample_rate = 0;
  int channels = 0;
  GstCaps *caps = gst_sample_get_caps(sample);
  if (caps) {
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (structure) {
      gst_structure_get_int(structure, "rate", &sample_rate);
      gst_structure_get_int(structure, "channels", &channels);
    }
  }

  // Map the buffer before locking to keep the critical section as short as possible.
  GstBuffer *buffer = gst_sample_get_buffer(sample);
  GstMapInfo map{};
  const bool buffer_mapped = buffer && gst_buffer_map(buffer, &map, GST_MAP_READ);

  GstFlowReturn flow_return = GST_FLOW_OK;
  {
    const QMutexLocker locker(&instance->mutex_state_);

    if (sample_rate > 0 && channels > 0 && instance->sample_rate_ == 0 && instance->channels_ == 0) {
      instance->sample_rate_ = sample_rate;
      instance->channels_ = channels;
      // Bound retained PCM bytes based on the actual decoded stream format.
      instance->max_pcm_bytes_ = static_cast<qint64>(sample_rate) * static_cast<qint64>(channels) * sizeof(int16_t) * kMaxPcmSeconds;
    }

    if (buffer_mapped) {
      qint64 bytes_to_write = static_cast<qint64>(map.size);
      if (instance->max_pcm_bytes_ > 0) {
        const qint64 remaining_bytes = instance->max_pcm_bytes_ - instance->buffer_.size();
        if (remaining_bytes <= 0) {
          instance->pcm_limit_reached_ = true;
          bytes_to_write = 0;
        }
        else if (bytes_to_write > remaining_bytes) {
          instance->pcm_limit_reached_ = true;
          bytes_to_write = remaining_bytes;
        }
      }

      if (bytes_to_write > 0) {
        instance->buffer_.write(reinterpret_cast<const char*>(map.data), bytes_to_write);
      }
    }

    if (instance->pcm_limit_reached_) {
      // Ask upstream to finish once we have enough decoded material.
      flow_return = GST_FLOW_EOS;
    }
  }

  if (buffer_mapped) gst_buffer_unmap(buffer, &map);
  gst_sample_unref(sample);

  return flow_return;

}
