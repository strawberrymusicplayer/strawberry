/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef MOODBARRENDERER_H
#define MOODBARRENDERER_H

#include <QMetaType>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QImage>
#include <QList>
#include <QColor>
#include <QPalette>
#include <QRect>
#include <QSize>

#include "constants/moodbarsettings.h"

class QPainter;

using ColorVector = QList<QColor>;

class MoodbarRenderer {
 public:
  static QString StyleName(const MoodbarSettings::Style style);

  static ColorVector Colors(const QByteArray &data, const MoodbarSettings::Style style, const QPalette &palette);
  static void Render(const ColorVector &colors, QPainter *p, const QRect rect);
  static QImage RenderToImage(const ColorVector &colors, const QSize size);

 private:
  explicit MoodbarRenderer();

  struct StyleProperties {
    explicit StyleProperties(const int threshold = 0, const int range_start = 0, const int range_delta = 0, const int sat = 0, const int val = 0)
        : threshold_(threshold),
          range_start_(range_start),
          range_delta_(range_delta),
          sat_(sat),
          val_(val) {}

    int threshold_;
    int range_start_;
    int range_delta_;
    int sat_;
    int val_;
  };
};

Q_DECLARE_METATYPE(QList<QColor>)

#endif  // MOODBARRENDERER_H
