/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MOODBARPIPELINE_H
#define MOODBARPIPELINE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include "includes/scoped_ptr.h"

class MoodbarBuilder;

// Creates moodbar data for a single local music file.
class MoodbarPipeline : public QObject {
  Q_OBJECT

 public:
  explicit MoodbarPipeline(const QUrl &url, QObject *parent = nullptr);
  ~MoodbarPipeline() override;

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
  static GstBusSyncReply BusCallbackSync(GstBus *bus, GstMessage *message, gpointer self);

 private:
  QUrl url_;
  GstElement *pipeline_;
  GstElement *convert_element_;

  ScopedPtr<MoodbarBuilder> builder_;

  bool success_;
  bool running_;
  QByteArray data_;
};

using MoodbarPipelinePtr = QSharedPointer<MoodbarPipeline>;

#endif  // MOODBARPIPELINE_H
