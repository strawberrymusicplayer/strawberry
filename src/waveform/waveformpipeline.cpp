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

#include "waveformpipeline.h"

#include <cstring>

#include <memory>

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <QObject>
#include <QCoreApplication>
#include <QThread>
#include <QString>
#include <QUrl>

#include "core/logging.h"
#include "core/signalchecker.h"
#include "utilities/threadutils.h"
#include "waveform/waveformbuilder.h"

using namespace Qt::Literals::StringLiterals;
using std::make_unique;

WaveformPipeline::WaveformPipeline(const QUrl &url, QObject *parent)
    : QObject(parent),
      url_(url),
      pipeline_(nullptr),
      convert_element_(nullptr),
      success_(false),
      running_(false) {}

WaveformPipeline::~WaveformPipeline() {

  Cleanup();

}

GstElement *WaveformPipeline::CreateElement(const QByteArray &factory_name) {

  GstElement *element = gst_element_factory_make(factory_name.constData(), nullptr);

  if (element) {
    gst_bin_add(GST_BIN(pipeline_), element);
  }
  else {
    qLog(Warning) << "Unable to create gstreamer element" << factory_name;
  }

  return element;

}

QByteArray WaveformPipeline::ToGstUrl(const QUrl &url) {

  if (url.isLocalFile() && !url.host().isEmpty()) {
    QString str = "file:////"_L1 + url.host() + url.path();
    return str.toUtf8();
  }

  return url.toEncoded();

}

void WaveformPipeline::Start() {

  Q_ASSERT(QThread::currentThread() == thread());
  Q_ASSERT(QThread::currentThread() != qApp->thread());

  Utilities::SetThreadIOPriority(Utilities::IoPriority::IOPRIO_CLASS_IDLE);

  if (pipeline_) {
    return;
  }

  pipeline_ = gst_pipeline_new("waveform-pipeline");

  GstElement *decodebin = CreateElement("uridecodebin");
  GstElement *convert_element = CreateElement("audioconvert");
  GstElement *resample = CreateElement("audioresample");
  GstElement *appsink = CreateElement("appsink");

  if (!decodebin || !convert_element || !resample || !appsink) {
    gst_object_unref(GST_OBJECT(pipeline_));
    pipeline_ = nullptr;
    Q_EMIT Finished(false);
    return;
  }

  // Join the converter and resampler together.
  if (!gst_element_link_many(convert_element, resample, nullptr)) {
    qLog(Error) << "Failed to link audioconvert to audioresample";
    gst_object_unref(GST_OBJECT(pipeline_));
    pipeline_ = nullptr;
    Q_EMIT Finished(false);
    return;
  }

  // Mono S16LE at the native sample rate: the "rate" key is deliberately
  // omitted so transients are preserved (unlike chromaprint's 11 kHz downmix).
  GstCaps *caps = gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING, "S16LE", "channels", G_TYPE_INT, 1, nullptr);
  const bool resample_to_sink_linked = gst_element_link_filtered(resample, appsink, caps);
  gst_caps_unref(caps);
  if (!resample_to_sink_linked) {
    qLog(Error) << "Failed to link audioresample to appsink with filter";
    gst_object_unref(GST_OBJECT(pipeline_));
    pipeline_ = nullptr;
    Q_EMIT Finished(false);
    return;
  }

  convert_element_ = convert_element;

  builder_ = make_unique<WaveformBuilder>();

  // appsink: pull-based, no playback-clock throttling.
  GstAppSinkCallbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.new_sample = NewBufferCallback;
  gst_app_sink_set_callbacks(reinterpret_cast<GstAppSink*>(appsink), &callbacks, this, nullptr);
  g_object_set(G_OBJECT(appsink), "sync", FALSE, nullptr);

  // Set properties.
  QByteArray gst_url = ToGstUrl(url_);
  g_object_set(decodebin, "uri", gst_url.constData(), nullptr);

  // Connect signals.
  CHECKED_GCONNECT(decodebin, "pad-added", &NewPadCallback, this);
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  if (bus) {
    gst_bus_set_sync_handler(bus, BusCallbackSync, this, nullptr);
    gst_object_unref(bus);
  }

  // Start playing.
  running_ = true;
  gst_element_set_state(pipeline_, GST_STATE_PLAYING);

}

void WaveformPipeline::ReportError(GstMessage *msg) {

  GError *error = nullptr;
  gchar *debugs = nullptr;

  gst_message_parse_error(msg, &error, &debugs);
  QString message;
  if (error) {
    message = QString::fromLocal8Bit(error->message);
    g_error_free(error);
  }
  g_free(debugs);

  qLog(Error) << "Error processing" << url_ << ":" << message;

}

void WaveformPipeline::NewPadCallback(GstElement *element, GstPad *pad, gpointer self) {

  Q_UNUSED(element)

  WaveformPipeline *instance = reinterpret_cast<WaveformPipeline*>(self);

  if (!instance->running_) {
    qLog(Warning) << "Received gstreamer callback after pipeline has stopped.";
    return;
  }

  GstPad *const audiopad = gst_element_get_static_pad(instance->convert_element_, "sink");
  if (!audiopad) return;

  if (GST_PAD_IS_LINKED(audiopad)) {
    qLog(Warning) << "audiopad is already linked, unlinking old pad";
    gst_pad_unlink(audiopad, GST_PAD_PEER(audiopad));
  }

  gst_pad_link(pad, audiopad);
  gst_object_unref(audiopad);

}

GstFlowReturn WaveformPipeline::NewBufferCallback(GstAppSink *app_sink, gpointer self) {

  WaveformPipeline *instance = reinterpret_cast<WaveformPipeline*>(self);

  GstSample *sample = gst_app_sink_pull_sample(app_sink);
  if (!sample) return GST_FLOW_ERROR;

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  if (buffer && instance->builder_) {
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      instance->builder_->AddSamples(reinterpret_cast<const qint16*>(map.data), static_cast<int>(map.size / sizeof(qint16)));
      gst_buffer_unmap(buffer, &map);
    }
  }
  gst_sample_unref(sample);

  return GST_FLOW_OK;

}

GstBusSyncReply WaveformPipeline::BusCallbackSync(GstBus *bus, GstMessage *message, gpointer self) {

  Q_UNUSED(bus)

  WaveformPipeline *instance = reinterpret_cast<WaveformPipeline*>(self);

  if (!instance->running_) return GST_BUS_PASS;

  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
      instance->Stop(true);
      break;

    case GST_MESSAGE_ERROR:
      instance->ReportError(message);
      instance->Stop(false);
      break;

    default:
      break;
  }

  return GST_BUS_PASS;

}

void WaveformPipeline::Stop(const bool success) {

  running_ = false;

  QMetaObject::invokeMethod(this, "Finish", Qt::QueuedConnection, Q_ARG(bool, success));

}

void WaveformPipeline::Finish(const bool success) {

  Q_ASSERT(QThread::currentThread() == thread());
  Q_ASSERT(QThread::currentThread() != qApp->thread());

  success_ = success;

  if (builder_) {
    data_ = builder_->Finish(kWaveformBaseCount);
    builder_.reset();
  }

  Cleanup();

  Q_EMIT Finished(success);

}

void WaveformPipeline::Cleanup() {

  running_ = false;

  if (pipeline_) {
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    if (bus) {
      gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
      gst_object_unref(bus);
    }

    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
  }

}
