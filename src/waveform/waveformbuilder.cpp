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

void WaveformBuilder::AddSamples(const qint16 *samples, const qsizetype count) {

  if (!samples || count <= 0) return;

  samples_.insert(samples_.end(), samples, samples + count);

  for (qsizetype i = 0; i < count; ++i) {
    const float magnitude = std::abs(static_cast<float>(samples[i]));
    if (magnitude > peak_) {
      peak_ = magnitude;
    }
  }

}

QByteArray WaveformBuilder::Finish(const int count) {

  // Precondition guards: an empty buffer must not produce a header-only blob,
  // and a non-positive count would divide by zero in the bucketing below.
  if (samples_.empty() || count <= 0) return QByteArray();

  QByteArray out;
  QDataStream stream(&out, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  stream.writeRawData(kWaveformMagic, 4);
  stream << kWaveformVersion;
  stream << static_cast<quint32>(count);
  // Encode a zero-peak (fully silent) track as a peak of 1 so the render-time
  // normalization (amp / peak) can never divide by zero. All amplitudes are 0
  // in that case, so the normalized envelope stays flat regardless.
  stream << (peak_ > 0.0f ? peak_ : 1.0f);

  const qsizetype samples_count = static_cast<qsizetype>(samples_.size());
  for (int i = 0; i < count; ++i) {
    const qsizetype start = static_cast<qsizetype>(i) * samples_count / count;
    qsizetype end = (static_cast<qsizetype>(i) + 1) * samples_count / count;
    end = std::min(end, samples_count);  // Never index past the buffer.

    if (end <= start) {
      // Bucket maps to no samples (count > samples_count for a short clip).
      // Emit a neutral (0, 0) pair rather than the inverted min/max sentinels.
      stream << static_cast<qint8>(0) << static_cast<qint8>(0);
      continue;
    }

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

bool WaveformBuilder::IsValidBlob(const QByteArray &data) {

  if (data.size() < kWaveformHeaderBytes) return false;
  if (data.left(4) != QByteArray::fromRawData(kWaveformMagic, 4)) return false;
  if (static_cast<quint8>(data[4]) != kWaveformVersion) return false;

  QDataStream stream(data);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
  stream.skipRawData(5);  // magic + version
  quint32 count = 0;
  stream >> count;
  if (stream.status() != QDataStream::Ok) return false;

  // The body holds one (min, max) qint8 pair per declared bucket.
  const qint64 expected = static_cast<qint64>(kWaveformHeaderBytes) + static_cast<qint64>(count) * 2;
  return static_cast<qint64>(data.size()) == expected;

}
