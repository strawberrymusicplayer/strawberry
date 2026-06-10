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

#ifndef WAVEFORMBUILDER_H
#define WAVEFORMBUILDER_H

#include <vector>

#include <QtGlobal>
#include <QByteArray>

// Number of min/max base pairs stored per track. The base envelope is kept at a
// higher resolution than any seekbar width so the renderer can re-bucket it to
// the actual pixel width without re-decoding (resolution-independent).
constexpr int kWaveformBaseCount = 2000;

// Pure transform that buffers decoded mono int16 PCM and reduces it to a fixed
// kWaveformBaseCount per-bucket min/max peak envelope, serialized as a versioned
// little-endian blob.
//
// The serialized layout is:
//   magic "SWVF" (4 bytes), version (quint8), count (quint32),
//   per-track peak (float32), then count pairs of (qint8 min, qint8 max).
// The header is little-endian with single-precision floats. Amplitudes are
// stored raw (un-normalized); normalization is applied at render time using the
// per-track peak.
class WaveformBuilder {
 public:
  WaveformBuilder() = default;

  // Appends count int16 samples and updates the running per-track peak.
  void AddSamples(const qint16 *samples, const int count);

  // Reduces the buffered samples into count min/max pairs and returns the
  // versioned blob. Returns an empty QByteArray when no samples were buffered.
  QByteArray Finish(const int count);

 private:
  std::vector<qint16> samples_;
  float peak_ = 0.0f;
};

#endif  // WAVEFORMBUILDER_H
