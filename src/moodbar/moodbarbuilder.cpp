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

#include <algorithm>
#include <cmath>

#include <QList>
#include <QByteArray>

#include "moodbarbuilder.h"
#include "core/logging.h"
#include "includes/arraysize.h"

namespace {

constexpr int sBarkBands[] = { 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500 };
constexpr int sBarkBandCount = arraysize(sBarkBands);

}  // namespace

MoodbarBuilder::MoodbarBuilder() : bands_(0), rate_hz_(0) {}

int MoodbarBuilder::BandFrequency(const int band) const {
  return ((rate_hz_ / 2) * band + rate_hz_ / 4) / bands_;
}

void MoodbarBuilder::Init(const int bands, const int rate_hz) {

  bands_ = bands;
  rate_hz_ = rate_hz;

  barkband_table_.clear();
  barkband_table_.reserve(bands + 1);

  int barkband = 0;
  for (int i = 0; i < bands + 1; ++i) {
    if (barkband < sBarkBandCount - 1 && BandFrequency(i) >= sBarkBands[barkband]) {
      barkband++;
    }

    barkband_table_.append(static_cast<uint>(barkband));
  }

}

void MoodbarBuilder::AddFrame(const double *magnitudes, const int size) {

  if (size > barkband_table_.length()) {
    return;
  }

  // Calculate total magnitudes for different bark bands.
  double bands[sBarkBandCount]{};
  for (int i = 0; i < size; ++i) {
    bands[barkband_table_[i]] += magnitudes[i];
  }

  // Now divide the bark bands into thirds and compute their total amplitudes.
  double rgb[] = { 0, 0, 0 };
  for (int i = 0; i < sBarkBandCount; ++i) {
    rgb[(i * 3) / sBarkBandCount] += bands[i] * bands[i];
  }

  frames_.append(Rgb(sqrt(rgb[0]), sqrt(rgb[1]), sqrt(rgb[2])));

}

void MoodbarBuilder::Normalize(QList<Rgb> *vals, double Rgb::*member) {

  const QList<Rgb> rgb_vals = *vals;

  double mini = vals->at(0).*member;
  double maxi = vals->at(0).*member;
  for (int i = 1; i < vals->count(); i++) {
    const double value = vals->at(i).*member;
    if (value > maxi) {
      maxi = value;
    }
    else if (value < mini) {
      mini = value;
    }
  }

  double avg = 0;
  for (const Rgb &rgb : rgb_vals) {
    const double value = rgb.*member;
    if (value != mini && value != maxi) {
      avg += value / static_cast<double>(vals->count());
    }
  }

  double tu = 0;
  double tb = 0;
  double avgu = 0;
  double avgb = 0;
  for (const Rgb &rgb : rgb_vals) {
    const double value = rgb.*member;
    if (value != mini && value != maxi) {
      if (value > avg) {
        avgu += value;
        tu++;
      }
      else {
        avgb += value;
        tb++;
      }
    }
  }
  avgu /= tu;
  avgb /= tb;

  tu = 0;
  tb = 0;
  double avguu = 0;
  double avgbb = 0;
  for (const Rgb &rgb : rgb_vals) {
    const double value = rgb.*member;
    if (value != mini && value != maxi) {
      if (value > avgu) {
        avguu += value;
        tu++;
      }
      else if (value < avgb) {
        avgbb += value;
        tb++;
      }
    }
  }
  avguu /= tu;
  avgbb /= tb;

  mini = std::max(avg + (avgb - avg) * 2, avgbb);
  maxi = std::min(avg + (avgu - avg) * 2, avguu);
  double delta = maxi - mini;
  if (delta == 0) {
    delta = 1;
  }

  for (auto it = vals->begin(); it != vals->end(); ++it) {
    double *value = &((*it).*member);
    *value = std::isfinite(*value) ? qBound(0.0, (*value - mini) / delta, 1.0) : 0;
  }

}

QByteArray MoodbarBuilder::Finish(const int width) {

  QByteArray ret;
  ret.resize(width * 3);
  char *data = ret.data();
  if (frames_.count() == 0) return ret;

  Normalize(&frames_, &Rgb::r);
  Normalize(&frames_, &Rgb::g);
  Normalize(&frames_, &Rgb::b);

  for (int i = 0; i < width; ++i) {
    Rgb rgb;
    const int start = static_cast<int>(i * frames_.count() / width);
    const int end = std::max(static_cast<int>((i + 1) * frames_.count() / width), start + 1);

    for (int j = start; j < end; j++) {
      const Rgb frame = frames_.value(j);
      rgb.r += frame.r * 255;
      rgb.g += frame.g * 255;
      rgb.b += frame.b * 255;
      frames_[j] = rgb;
    }

    const int n = end - start;

    *(data++) = static_cast<char>(rgb.r / n);
    *(data++) = static_cast<char>(rgb.g / n);
    *(data++) = static_cast<char>(rgb.b / n);

  }

  return ret;

}
