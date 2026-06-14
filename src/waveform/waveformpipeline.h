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

#ifndef WAVEFORMPIPELINE_H
#define WAVEFORMPIPELINE_H

#include <atomic>

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "includes/scoped_ptr.h"

class WaveformBuilder;

// Decodes a single local music file to native-rate mono S16LE PCM via a
// GStreamer appsink pre-pass on a worker thread, feeding each decoded buffer to
// a WaveformBuilder. On EOS the builder produces a versioned min/max envelope
// blob, exposed via data(); Finished(true) is emitted on success.
class WaveformPipeline : public QObject {
  Q_OBJECT

 public:
  explicit WaveformPipeline(const QUrl &url, QObject *parent = nullptr);
  ~WaveformPipeline() override;

  bool success() const { return success_; }
  const QByteArray &data() const { return data_; }

  Q_INVOKABLE void Start();

 Q_SIGNALS:
  void Finished(const bool success);

 private:
  GstElement *CreateElement(const QByteArray &factory_name);

  QByteArray ToGstUrl(const QUrl &url);
  void ReportError(GstMessage *msg);
  void Stop(const bool success);
  Q_INVOKABLE void Finish(const bool success);
  void Cleanup();

  static void NewPadCallback(GstElement *element, GstPad *pad, gpointer self);
  static GstFlowReturn NewBufferCallback(GstAppSink *app_sink, gpointer self);
  static GstBusSyncReply BusCallbackSync(GstBus *bus, GstMessage *message, gpointer self);

 private:
  QUrl url_;
  GstElement *pipeline_;
  GstElement *convert_element_;

  ScopedPtr<WaveformBuilder> builder_;

  bool success_;
  std::atomic<bool> running_;
  QByteArray data_;
};

using WaveformPipelinePtr = QSharedPointer<WaveformPipeline>;

#endif  // WAVEFORMPIPELINE_H
