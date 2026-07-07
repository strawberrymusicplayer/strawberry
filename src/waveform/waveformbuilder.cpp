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

// Number of min/max base pairs stored per track.
// The base envelope is kept at a higher resolution than any seekbar width so the renderer can re-bucket it to the actual pixel width without re-decoding (resolution-independent).
const int WaveformBuilder::kWaveformBaseCount = 2000;

// Serialization header constants shared by the writer, the cache reader and the tests so the magic, version and header size cannot drift between them.
const char WaveformBuilder::kWaveformMagic[] = "SWVF";
const quint8 WaveformBuilder::kWaveformVersion = 1;
// magic (4) + version (1) + count (4) + peak (4).
const int WaveformBuilder::kWaveformHeaderBytes = 13;

// Exact serialized blob size for the fixed-resolution format.
// Used both to validate blobs and to bound reads of untrusted sidecar/cache files before they are deserialized.
const qint64 WaveformBuilder::kWaveformBlobBytes = static_cast<qint64>(kWaveformHeaderBytes) + static_cast<qint64>(kWaveformBaseCount) * 2;

namespace {
// Upper bound on the number of working min/max buckets retained while streaming PCM through the builder.
// This caps memory at O(1) regardless of track length: when the bound is reached adjacent buckets are merged (halving the temporal resolution and doubling the samples-per-bucket), so the working envelope always stays at >= the output resolution.
// Kept even so pairwise folding is exact.
constexpr int kWaveformMaxWorkingBuckets = WaveformBuilder::kWaveformBaseCount * 2;
}  // namespace

WaveformBuilder::WaveformBuilder()
    : samples_per_bucket_(1),
      current_bucket_fill_(0),
      current_min_(0),
      current_max_(0),
      peak_(0.0F) {}

void WaveformBuilder::AddSamples(const qint16 *samples, const qsizetype count) {

  if (!samples || count <= 0) return;

  for (qsizetype i = 0; i < count; ++i) {
    const qint16 sample = samples[i];

    const float magnitude = std::abs(static_cast<float>(sample));
    if (magnitude > peak_) {
      peak_ = magnitude;
    }

    // Accumulate into the in-progress working bucket.
    if (current_bucket_fill_ == 0) {
      current_min_ = sample;
      current_max_ = sample;
    }
    else {
      current_min_ = std::min(current_min_, sample);
      current_max_ = std::max(current_max_, sample);
    }
    ++current_bucket_fill_;

    // Finalize the bucket once it holds samples_per_bucket_ samples.
    if (current_bucket_fill_ >= samples_per_bucket_) {
      bucket_min_.push_back(current_min_);
      bucket_max_.push_back(current_max_);
      current_bucket_fill_ = 0;
      // Cap memory: when the working set is full, halve the resolution.
      if (static_cast<int>(bucket_min_.size()) >= kWaveformMaxWorkingBuckets) {
        FoldBuckets();
      }
    }
  }

}

void WaveformBuilder::FoldBuckets() {

  // Merge adjacent (min, max) pairs, keeping the extremes, so the working set shrinks by half while each surviving bucket now covers twice as many samples.
  // Triggered at an even bucket count, but the odd carry below keeps it correct regardless.
  const qsizetype n = static_cast<qsizetype>(bucket_min_.size());
  const qsizetype half = n / 2;
  for (qsizetype i = 0; i < half; ++i) {
    bucket_min_[i] = std::min(bucket_min_[2 * i], bucket_min_[2 * i + 1]);
    bucket_max_[i] = std::max(bucket_max_[2 * i], bucket_max_[2 * i + 1]);
  }
  if (n % 2 != 0) {
    bucket_min_[half] = bucket_min_[n - 1];
    bucket_max_[half] = bucket_max_[n - 1];
    bucket_min_.resize(half + 1);
    bucket_max_.resize(half + 1);
  }
  else {
    bucket_min_.resize(half);
    bucket_max_.resize(half);
  }

  samples_per_bucket_ *= 2;

}

QByteArray WaveformBuilder::Finish(const int count) {

  // A non-positive count would divide by zero in the bucketing below.
  if (count <= 0) return QByteArray();

  // Flush any partially filled trailing bucket so the last samples are kept.
  if (current_bucket_fill_ > 0) {
    bucket_min_.push_back(current_min_);
    bucket_max_.push_back(current_max_);
    current_bucket_fill_ = 0;
  }

  // An empty buffer must not produce a header-only blob.
  if (bucket_min_.empty()) return QByteArray();

  QByteArray out;
  QDataStream stream(&out, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  stream.writeRawData(kWaveformMagic, 4);
  stream << kWaveformVersion;
  stream << static_cast<quint32>(count);
  // Encode a zero-peak (fully silent) track as a peak of 1 so the render-time normalization (amp / peak) can never divide by zero.
  // All amplitudes are 0 in that case, so the normalized envelope stays flat regardless.
  stream << (peak_ > 0.0F ? peak_ : 1.0F);

  // Re-bucket the working envelope into exactly count output buckets.
  const qsizetype bucket_count = static_cast<qsizetype>(bucket_min_.size());
  for (int i = 0; i < count; ++i) {
    const qsizetype start = static_cast<qsizetype>(i) * bucket_count / count;
    qsizetype end = (static_cast<qsizetype>(i) + 1) * bucket_count / count;
    end = std::min(end, bucket_count);  // Never index past the working set.

    if (end <= start) {
      // Output bucket maps to no working bucket (count > bucket_count for a short clip).
      // Emit a neutral (0, 0) pair, not the inverted sentinels.
      stream << static_cast<qint8>(0) << static_cast<qint8>(0);
      continue;
    }

    qint16 mn = std::numeric_limits<qint16>::max();
    qint16 mx = std::numeric_limits<qint16>::min();
    for (qsizetype j = start; j < end; ++j) {
      mn = std::min(mn, bucket_min_[j]);
      mx = std::max(mx, bucket_max_[j]);
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

  // The format is fixed-resolution.
  // Reject any blob that does not declare exactly kWaveformBaseCount buckets: an attacker-influenced count would otherwise drive a huge (or, beyond INT_MAX, negative) allocation in the renderer when it sizes its column vector from this field.
  if (count != static_cast<quint32>(kWaveformBaseCount)) return false;

  // The body holds one (min, max) qint8 pair per bucket.
  return static_cast<qint64>(data.size()) == kWaveformBlobBytes;

}
