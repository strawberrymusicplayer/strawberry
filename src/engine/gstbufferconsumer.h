/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef GSTBUFFERCONSUMER_H
#define GSTBUFFERCONSUMER_H

#include "config.h"

#include <gst/gstbuffer.h>

#include <QString>

class GstEnginePipeline;

class GstBufferConsumer {
public:
  virtual ~GstBufferConsumer() {}

  // This is called in some unspecified GStreamer thread.
  // Ownership of the buffer is transferred to the BufferConsumer and it should gst_buffer_unref it.
  virtual void ConsumeBuffer(GstBuffer *buffer, const int pipeline_id, const QString &format) = 0;
};

#endif // GSTBUFFERCONSUMER_H
