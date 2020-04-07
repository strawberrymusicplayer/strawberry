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

#ifndef GSTELEMENTDELETER_H
#define GSTELEMENTDELETER_H

#include "config.h"

#include <gst/gst.h>

#include <QObject>
#include <QString>

class GstElementDeleter : public QObject {
  Q_OBJECT

 public:
  explicit GstElementDeleter(QObject *parent = nullptr);

  // If you call this function with any gstreamer element, the element will get deleted in the main thread.
  // This is useful if you need to delete an element from its own callback.
  // It's in a separate object so *your* object (GstEnginePipeline) can be destroyed,
  // and the element that you scheduled for deletion is still deleted later regardless.
  void DeleteElementLater(GstElement *element);

 private slots:
  void DeleteElement(GstElement *element);
};

#endif  // GSTELEMENTDELETER_H
