/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2014, David Sansome <me@davidsansome.com>
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MOODBARBUILDER_H
#define MOODBARBUILDER_H

#include <QtGlobal>
#include <QList>
#include <QByteArray>

class MoodbarBuilder {
 public:
  explicit MoodbarBuilder();

  void Init(const int bands, const int rate_hz);
  void AddFrame(const double *magnitudes, const int size);
  QByteArray Finish(const int width);

 private:
  struct Rgb {
    Rgb() : r(0), g(0), b(0) {}
    Rgb(const double r_, const double g_, const double b_) : r(r_), g(g_), b(b_) {}

    double r, g, b;
  };

  int BandFrequency(const int band) const;
  static void Normalize(QList<Rgb> *vals, double Rgb::*member);

  QList<uint> barkband_table_;
  int bands_;
  int rate_hz_;

  QList<Rgb> frames_;
};

#endif  // MOODBARBUILDER_H
