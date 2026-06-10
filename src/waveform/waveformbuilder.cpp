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

#include <algorithm>
#include <cmath>
#include <limits>

#include <QByteArray>
#include <QIODevice>
#include <QDataStream>

#include "waveformbuilder.h"

using namespace Qt::Literals::StringLiterals;

void WaveformBuilder::AddSamples(const qint16 *samples, const int count) {

  if (!samples || count <= 0) return;

  samples_.insert(samples_.end(), samples, samples + count);

  for (int i = 0; i < count; ++i) {
    const float magnitude = std::abs(static_cast<float>(samples[i]));
    if (magnitude > peak_) {
      peak_ = magnitude;
    }
  }

}

QByteArray WaveformBuilder::Finish(const int count) {

  // Empty-input short-circuit: the loader must not cache a header-only blob.
  if (samples_.empty()) return QByteArray();

  QByteArray out;
  QDataStream stream(&out, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  stream.writeRawData("SWVF", 4);
  stream << static_cast<quint8>(1);
  stream << static_cast<quint32>(count);
  stream << static_cast<float>(peak_);

  const qsizetype samples_count = static_cast<qsizetype>(samples_.size());
  for (int i = 0; i < count; ++i) {
    const qsizetype start = static_cast<qsizetype>(i) * samples_count / count;
    const qsizetype end = std::max<qsizetype>((static_cast<qsizetype>(i) + 1) * samples_count / count, start + 1);

    qint16 mn = std::numeric_limits<qint16>::max();
    qint16 mx = std::numeric_limits<qint16>::min();
    for (qsizetype j = start; j < end; ++j) {
      mn = std::min(mn, samples_[j]);
      mx = std::max(mx, samples_[j]);
    }

    // Raw int8 quantization (high byte). No normalization at storage time.
    stream << static_cast<qint8>(mn >> 8) << static_cast<qint8>(mx >> 8);
  }

  return out;

}
