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

// Pure transform that reduces decoded mono int16 PCM, streamed in incrementally, to a fixed kWaveformBaseCount per-bucket min/max peak envelope, serialized as a versioned little-endian blob.
//
// Reduction is streaming: samples are accumulated into a bounded set of working min/max buckets so peak memory does not grow with track length.
// When the cap is reached adjacent buckets are merged.
//
// The serialized layout is:
//   magic "SWVF" (4 bytes), version (quint8), count (quint32),
//   per-track peak (float32), then count pairs of (qint8 min, qint8 max).
// The header is little-endian with single-precision floats.
// Amplitudes are stored raw (un-normalized); normalization is applied at render time using the per-track peak.
class WaveformBuilder {
 public:
  explicit WaveformBuilder();

  static const int kWaveformBaseCount;
  static const char kWaveformMagic[];
  static const quint8 kWaveformVersion;
  static const int kWaveformHeaderBytes;
  static const qint64 kWaveformBlobBytes;

  // Appends count int16 samples, folding them into the bounded working buckets and updating the running per-track peak.
  void AddSamples(const qint16 *samples, const qsizetype count);

  // Reduces the working buckets into count min/max pairs and returns the versioned blob.
  // Returns an empty QByteArray when no samples were buffered or when count is not positive.
  QByteArray Finish(const int count);

  // Validates a serialized blob: checks the magic, a known version and that it declares exactly kWaveformBaseCount buckets with a matching body length.
  // Returns true only for a blob that can be safely deserialized by the consumer.
  static bool IsValidBlob(const QByteArray &data);

 private:
  // Halves the working resolution by merging adjacent (min, max) bucket pairs.
  void FoldBuckets();

 private:
  std::vector<qint16> bucket_min_;
  std::vector<qint16> bucket_max_;
  qint64 samples_per_bucket_;
  qint64 current_bucket_fill_;
  qint16 current_min_;
  qint16 current_max_;
  float peak_;
};

#endif  // WAVEFORMBUILDER_H
