/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QThread>
#include <QIODevice>
#include <QByteArray>
#include <QString>
#include <QUrl>
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
// Bound the amount of decoded PCM we keep in memory while fingerprinting.
constexpr int kMaxPcmSeconds = 120;

QByteArray ToGstUrl(const QUrl &url) {
  if (url.isLocalFile() && !url.host().isEmpty()) {
    const QString str = "file:////"_L1 + url.host() + url.path();
    return str.toUtf8();
  }
  return url.toEncoded();
}
}  // namespace

Chromaprinter::Chromaprinter(const QString &filename)
    : filename_(filename),
      sample_rate_(0),
      channels_(0),
      max_pcm_bytes_(0),
      pcm_limit_reached_(false),
      convert_element_(nullptr) {}

GstElement *Chromaprinter::CreateElement(const QString &factory_name, GstElement *bin) {

  GstElement *ret = gst_element_factory_make(factory_name.toLatin1().constData(), factory_name.toLatin1().constData());

  if (ret && bin) gst_bin_add(GST_BIN(bin), ret);

  if (!ret) {
    qLog(Warning) << "Couldn't create the gstreamer element" << factory_name;
  }

  return ret;

}

QString Chromaprinter::CreateFingerprint() {

  Q_ASSERT(QThread::currentThread() != qApp->thread());

  last_error_.clear();
  sample_rate_ = 0;
  channels_ = 0;
  max_pcm_bytes_ = 0;
  pcm_limit_reached_ = false;
  auto set_error = [this](const QString &error) {
    last_error_ = error;
    qLog(Warning) << "Chromaprinter:" << error << "File:" << filename_;
  };

  if (!buffer_.open(QIODevice::WriteOnly)) {
    set_error(QStringLiteral("Could not open temporary fingerprint buffer."));
    return QString();
  }

  GstElement *pipeline = gst_pipeline_new("pipeline");
  if (!pipeline) {
    buffer_.close();
    set_error(QStringLiteral("Could not create GStreamer pipeline."));
    return QString();
  }

  GstElement *decode = CreateElement(u"uridecodebin"_s, pipeline);
  GstElement *convert = CreateElement(u"audioconvert"_s, pipeline);
  GstElement *queue = CreateElement(u"queue2"_s, pipeline);
  GstElement *sink = CreateElement(u"appsink"_s, pipeline);

  if (!decode || !convert || !queue || !sink) {
    gst_object_unref(pipeline);
    buffer_.close();
    set_error(QStringLiteral("Could not create one or more GStreamer elements (uridecodebin/audioconvert/queue2/appsink)."));
    return QString();
  }

  convert_element_ = convert;

  // Connect the elements
  // Request 16-bit PCM and let GStreamer keep a supported sample rate.
  GstCaps *caps = gst_caps_new_simple("audio/x-raw",
                                      "format", G_TYPE_STRING, "S16LE",
                                      "layout", G_TYPE_STRING, "interleaved",
                                      "channels", GST_TYPE_INT_RANGE, 1, 2,
                                      nullptr);
  const gboolean sink_link_ok = gst_element_link_filtered(convert, queue, caps);
  gst_caps_unref(caps);
  if (!sink_link_ok || !gst_element_link_many(queue, sink, nullptr)) {
    gst_object_unref(pipeline);
    buffer_.close();
    set_error(QStringLiteral("Failed to link audioconvert/queue2/appsink with required audio caps."));
    return QString();
  }

  GstAppSinkCallbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.new_sample = NewBufferCallback;
  gst_app_sink_set_callbacks(reinterpret_cast<GstAppSink*>(sink), &callbacks, this, nullptr);
  g_object_set(G_OBJECT(sink), "sync", FALSE, nullptr);
  g_object_set(G_OBJECT(sink), "emit-signals", TRUE, nullptr);

  // Use URI-based decodebin for more robust demux/decoder selection.
  const QByteArray gst_url = ToGstUrl(QUrl::fromLocalFile(filename_));
  g_object_set(G_OBJECT(decode), "uri", gst_url.constData(), nullptr);

  // Allow queueing of decoded audio to avoid starving appsink callback.
  // Keep the in-pipeline queue bounded as an additional memory guard.
  g_object_set(G_OBJECT(queue), "max-size-time", 10 * GST_SECOND, nullptr);
  g_object_set(G_OBJECT(queue), "max-size-buffers", 0, nullptr);
  g_object_set(G_OBJECT(queue), "max-size-bytes", 8 * 1024 * 1024, nullptr);

  // Connect signals
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  CHECKED_GCONNECT(decode, "pad-added", &NewPadCallback, this);

  QElapsedTimer time;
  time.start();

  // Start playing
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  bool finished = false;
  QElapsedTimer bus_timer;
  bus_timer.start();

  while (!finished && bus_timer.elapsed() < kTimeoutSecs * 1000) {
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, 200 * GST_MSECOND,
        static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR | GST_MESSAGE_WARNING));
    if (!msg) continue;

    if (msg->type == GST_MESSAGE_ERROR) {
      GError *error = nullptr;
      gchar *debugs = nullptr;
      gst_message_parse_error(msg, &error, &debugs);
      if (error) {
        const QString message = QString::fromLocal8Bit(error->message);
        g_error_free(error);
        qLog(Debug) << "Error processing" << filename_ << ":" << message;
        set_error(QStringLiteral("GStreamer decode error: %1").arg(message));
      }
      if (debugs) free(debugs);
      finished = true;
    }
    else if (msg->type == GST_MESSAGE_WARNING) {
      GError *warning = nullptr;
      gchar *debugs = nullptr;
      gst_message_parse_warning(msg, &warning, &debugs);
      if (warning) {
        const QString message = QString::fromLocal8Bit(warning->message);
        g_error_free(warning);
        qLog(Warning) << "Chromaprinter warning for" << filename_ << ":" << message;
        if (last_error_.isEmpty()) {
          last_error_ = QStringLiteral("GStreamer warning: %1").arg(message);
        }
      }
      if (debugs) free(debugs);
    }
    else if (msg->type == GST_MESSAGE_EOS) {
      finished = true;
    }

    gst_message_unref(msg);
  }

  if (!finished) {
    qLog(Warning) << "Chromaprinter: Timed out waiting for EOS/error after" << kTimeoutSecs << "seconds for" << filename_;
  }

  const qint64 decode_time = time.restart();

  buffer_.close();

  // Generate fingerprint from recorded buffer data
  QByteArray data = buffer_.data();
  if (data.isEmpty()) {
    // Usually means decodebin never produced a usable raw audio pad.
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    QString error_message = QStringLiteral("No decoded audio samples were produced by GStreamer.");
    const QString callback_error = last_error_.trimmed();
    if (!callback_error.isEmpty() && !error_message.contains(callback_error, Qt::CaseInsensitive)) {
      error_message += QStringLiteral(" %1").arg(callback_error);
    }
    set_error(error_message);
    return QString();
  }
  if (pcm_limit_reached_) {
    qLog(Debug) << "Chromaprinter: Reached in-memory PCM cap for" << filename_ << "bytes stored" << data.size();
  }

  if (sample_rate_ <= 0 || channels_ <= 0) {
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    QString error_message = QStringLiteral("No valid PCM format metadata was received from GStreamer.");
    const QString callback_error = last_error_.trimmed();
    if (!callback_error.isEmpty() && !error_message.contains(callback_error, Qt::CaseInsensitive)) {
      error_message += QStringLiteral(" %1").arg(callback_error);
    }
    set_error(error_message);
    return QString();
  }

  ChromaprintContext *chromaprint = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
  if (!chromaprint) {
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    set_error(QStringLiteral("Failed to create Chromaprint context."));
    return QString();
  }

  if (chromaprint_start(chromaprint, sample_rate_, channels_) != 1) {
    chromaprint_free(chromaprint);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    set_error(QStringLiteral("Chromaprint could not start (sample rate %1, channels %2).").arg(sample_rate_).arg(channels_));
    return QString();
  }
  if (chromaprint_feed(chromaprint, reinterpret_cast<int16_t*>(data.data()), static_cast<int>(data.size() / 2)) != 1) {
    chromaprint_free(chromaprint);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    set_error(QStringLiteral("Chromaprint failed while processing decoded samples."));
    return QString();
  }
  if (chromaprint_finish(chromaprint) != 1) {
    chromaprint_free(chromaprint);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    set_error(QStringLiteral("Chromaprint could not finalize fingerprint."));
    return QString();
  }

  u_int32_t *fprint = nullptr;
  int size = 0;
  int ret = chromaprint_get_raw_fingerprint(chromaprint, &fprint, &size);
  QByteArray fingerprint;
  if (ret == 1) {
    char *encoded = nullptr;
    int encoded_size = 0;
    ret = chromaprint_encode_fingerprint(fprint, size, CHROMAPRINT_ALGORITHM_DEFAULT, &encoded, &encoded_size, 1);
    if (ret == 1) {
      fingerprint.append(encoded, encoded_size);
      chromaprint_dealloc(encoded);
    }
    else {
      set_error(QStringLiteral("Chromaprint failed to encode generated fingerprint."));
    }
    chromaprint_dealloc(fprint);
  }
  else {
    set_error(QStringLiteral("Chromaprint did not return a raw fingerprint."));
  }
  chromaprint_free(chromaprint);

  const qint64 codegen_time = time.elapsed();

  qLog(Debug) << "Decode time:" << decode_time << "Codegen time:" << codegen_time;

  // Cleanup
  callbacks.new_sample = nullptr;
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);

  const QString fingerprint_string = QString::fromUtf8(fingerprint);
  if (fingerprint_string.size() < kMinimumEncodedFingerprintLength) {
    set_error(QStringLiteral("Generated fingerprint is too short (%1 bytes).").arg(fingerprint_string.size()));
    return QString();
  }

  qLog(Debug) << "Chromaprinter: Generated fingerprint for" << filename_ << "length" << fingerprint_string.size();

  return fingerprint_string;

}

void Chromaprinter::NewPadCallback(GstElement *element, GstPad *pad, gpointer data) {

  Q_UNUSED(element)

  Chromaprinter *instance = reinterpret_cast<Chromaprinter*>(data);
  if (!instance || !instance->convert_element_) {
    return;
  }
  GstPad *const audiopad = gst_element_get_static_pad(instance->convert_element_, "sink");
  if (!audiopad) {
    if (instance->last_error_.isEmpty()) {
      instance->last_error_ = QStringLiteral("Could not obtain audioconvert sink pad.");
    }
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

  if (GST_PAD_IS_LINKED(audiopad)) {
    gst_object_unref(audiopad);
    return;
  }

  const GstPadLinkReturn ret = gst_pad_link(pad, audiopad);
  if (ret != GST_PAD_LINK_OK) {
    QString reason;
    if (!caps_name.isEmpty()) {
      reason = QStringLiteral("Failed to link decodebin pad '%1' to audioconvert (code %2).").arg(caps_name).arg(static_cast<int>(ret));
    }
    else {
      reason = QStringLiteral("Failed to link decodebin pad to audioconvert (code %1).").arg(static_cast<int>(ret));
    }
    if (instance->last_error_.isEmpty()) {
      instance->last_error_ = reason;
    }
    qLog(Warning) << "Chromaprinter:" << reason << "File:" << instance->filename_;
  }
  gst_object_unref(audiopad);

}

GstFlowReturn Chromaprinter::NewBufferCallback(GstAppSink *app_sink, gpointer self) {

  Chromaprinter *me = reinterpret_cast<Chromaprinter*>(self);

  GstSample *sample = gst_app_sink_pull_sample(app_sink);
  if (!sample) return GST_FLOW_ERROR;

  GstCaps *caps = gst_sample_get_caps(sample);
  if (caps) {
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (structure) {
      int sample_rate = 0;
      int channels = 0;
      const bool has_sample_rate = gst_structure_get_int(structure, "rate", &sample_rate);
      const bool has_channels = gst_structure_get_int(structure, "channels", &channels);
      if (has_sample_rate && has_channels && sample_rate > 0 && channels > 0) {
        if (me->sample_rate_ == 0 && me->channels_ == 0) {
          me->sample_rate_ = sample_rate;
          me->channels_ = channels;
          // Bound retained PCM bytes based on the actual decoded stream format.
          me->max_pcm_bytes_ = static_cast<qint64>(sample_rate) * static_cast<qint64>(channels) * sizeof(int16_t) * kMaxPcmSeconds;
        }
      }
    }
  }

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  if (buffer) {
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      qint64 bytes_to_write = static_cast<qint64>(map.size);
      if (me->max_pcm_bytes_ > 0) {
        const qint64 remaining_bytes = me->max_pcm_bytes_ - me->buffer_.size();
        if (remaining_bytes <= 0) {
          me->pcm_limit_reached_ = true;
          bytes_to_write = 0;
        }
        else if (bytes_to_write > remaining_bytes) {
          me->pcm_limit_reached_ = true;
          bytes_to_write = remaining_bytes;
        }
      }

      if (bytes_to_write > 0) {
        me->buffer_.write(reinterpret_cast<const char*>(map.data), bytes_to_write);
      }
      gst_buffer_unmap(buffer, &map);
    }
  }
  gst_sample_unref(sample);

  if (me->pcm_limit_reached_) {
    // Ask upstream to finish once we have enough decoded material.
    return GST_FLOW_EOS;
  }

  return GST_FLOW_OK;

}
