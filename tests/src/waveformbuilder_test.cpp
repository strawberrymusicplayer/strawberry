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

#include <vector>

#include "gtest_include.h"

#include <QtGlobal>
#include <QByteArray>
#include <QIODevice>
#include <QDataStream>

#include "test_utils.h"

#include "waveform/waveformbuilder.h"

namespace {

// Reads back the versioned header (magic, version, count, peak) from a blob, leaving the stream positioned at the start of the int8 body.
void ReadHeader(QDataStream &stream, QByteArray &magic, quint8 &version, quint32 &count, float &peak) {
  magic.resize(4);
  stream.readRawData(magic.data(), 4);
  stream >> version;
  stream >> count;
  stream >> peak;
}

}  // namespace

TEST(WaveformBuilderTest, EmptyInputProducesEmptyBlob) {

  WaveformBuilder builder;
  const QByteArray data = builder.Finish(WaveformBuilder::kWaveformBaseCount);
  EXPECT_TRUE(data.isEmpty());

}

TEST(WaveformBuilderTest, HeaderLayoutAndBodyLength) {

  WaveformBuilder builder;
  std::vector<qint16> samples(8192, 0);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] = static_cast<qint16>((i % 2 == 0) ? 12000 : -8000);
  }
  builder.AddSamples(samples.data(), static_cast<int>(samples.size()));

  const QByteArray data = builder.Finish(WaveformBuilder::kWaveformBaseCount);

  EXPECT_EQ(data.left(4), QByteArray("SWVF"));

  QDataStream stream(data);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  QByteArray magic;
  quint8 version = 0;
  quint32 count = 0;
  float peak = 0.0f;
  ReadHeader(stream, magic, version, count, peak);

  EXPECT_EQ(magic, QByteArray("SWVF"));
  EXPECT_EQ(version, static_cast<quint8>(1));
  EXPECT_EQ(count, static_cast<quint32>(WaveformBuilder::kWaveformBaseCount));
  EXPECT_GT(peak, 0.0f);

  // Header = 4 (magic) + 1 (version) + 4 (count) + 4 (peak) = 13 bytes; body = count * 2 (one qint8 min + one qint8 max per bucket).
  const int header_bytes = 4 + 1 + 4 + 4;
  EXPECT_EQ(data.size(), header_bytes + WaveformBuilder::kWaveformBaseCount * 2);

}

TEST(WaveformBuilderTest, NeverAveragesPreservingExtremes) {

  // A single bucket sub-range containing both a deep negative trough and a high positive peak must retain both extremes, not average them toward zero.
  WaveformBuilder builder;
  std::vector<qint16> samples;
  samples.reserve(WaveformBuilder::kWaveformBaseCount * 4);
  for (int i = 0; i < WaveformBuilder::kWaveformBaseCount * 4; ++i) {
    samples.push_back(static_cast<qint16>((i % 2 == 0) ? 30000 : -30000));
  }
  builder.AddSamples(samples.data(), static_cast<int>(samples.size()));

  const QByteArray data = builder.Finish(WaveformBuilder::kWaveformBaseCount);

  QDataStream stream(data);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  QByteArray magic;
  quint8 version = 0;
  quint32 count = 0;
  float peak = 0.0f;
  ReadHeader(stream, magic, version, count, peak);

  qint8 mn = 0;
  qint8 mx = 0;
  stream >> mn >> mx;

  EXPECT_LT(mn, 0);
  EXPECT_GT(mx, 0);

}

TEST(WaveformBuilderTest, NonPositiveCountProducesEmptyBlob) {

  // A non-positive count would divide by zero in the bucketing loop; Finish must short-circuit to an empty blob instead.
  WaveformBuilder builder;
  std::vector<qint16> samples(128, 1000);
  builder.AddSamples(samples.data(), static_cast<qsizetype>(samples.size()));

  EXPECT_TRUE(builder.Finish(0).isEmpty());
  EXPECT_TRUE(builder.Finish(-5).isEmpty());

}

TEST(WaveformBuilderTest, SparseInputDoesNotReadOutOfBounds) {

  // Fewer samples than buckets: many buckets map to no samples and must emit a neutral (0, 0) pair rather than reading past the buffer or emitting the inverted min > max sentinels.
  // The blob length must still match the header.
  WaveformBuilder builder;
  std::vector<qint16> samples(50);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] = static_cast<qint16>((i % 2 == 0) ? 20000 : -20000);
  }
  builder.AddSamples(samples.data(), static_cast<qsizetype>(samples.size()));

  const QByteArray data = builder.Finish(WaveformBuilder::kWaveformBaseCount);

  EXPECT_TRUE(WaveformBuilder::IsValidBlob(data));
  EXPECT_EQ(data.size(), WaveformBuilder::kWaveformHeaderBytes + WaveformBuilder::kWaveformBaseCount * 2);

  QDataStream stream(data);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  QByteArray magic;
  quint8 version = 0;
  quint32 count = 0;
  float peak = 0.0f;
  ReadHeader(stream, magic, version, count, peak);

  // Every emitted pair must satisfy min <= max (no inverted sentinels).
  for (quint32 i = 0; i < count; ++i) {
    qint8 mn = 0;
    qint8 mx = 0;
    stream >> mn >> mx;
    EXPECT_LE(mn, mx);
  }

}

TEST(WaveformBuilderTest, SilentTrackEncodesNonZeroPeak) {

  // An all-zero (silent) track has peak 0, which would divide by zero at render time.
  // Finish must encode a non-zero peak and a flat (0, 0) envelope.
  WaveformBuilder builder;
  std::vector<qint16> samples(8192, 0);
  builder.AddSamples(samples.data(), static_cast<qsizetype>(samples.size()));

  const QByteArray data = builder.Finish(WaveformBuilder::kWaveformBaseCount);

  EXPECT_TRUE(WaveformBuilder::IsValidBlob(data));

  QDataStream stream(data);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

  QByteArray magic;
  quint8 version = 0;
  quint32 count = 0;
  float peak = 0.0f;
  ReadHeader(stream, magic, version, count, peak);

  EXPECT_GT(peak, 0.0f);

  qint8 mn = 0;
  qint8 mx = 0;
  stream >> mn >> mx;
  EXPECT_EQ(mn, 0);
  EXPECT_EQ(mx, 0);

}

TEST(WaveformBuilderTest, OddSampleCountProducesValidBlob) {

  // An odd, small buffer (not a clean multiple of the bucket count) must still produce a structurally valid blob.
  WaveformBuilder builder;
  std::vector<qint16> samples(2001);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] = static_cast<qint16>((i % 3 == 0) ? 15000 : -5000);
  }
  builder.AddSamples(samples.data(), static_cast<qsizetype>(samples.size()));

  const QByteArray data = builder.Finish(WaveformBuilder::kWaveformBaseCount);

  EXPECT_TRUE(WaveformBuilder::IsValidBlob(data));
  EXPECT_EQ(data.size(), WaveformBuilder::kWaveformHeaderBytes + WaveformBuilder::kWaveformBaseCount * 2);

}

TEST(WaveformBuilderTest, IsValidBlobRejectsMalformedBlobs) {

  WaveformBuilder builder;
  std::vector<qint16> samples(8192, 10000);
  builder.AddSamples(samples.data(), static_cast<qsizetype>(samples.size()));
  const QByteArray good = builder.Finish(WaveformBuilder::kWaveformBaseCount);
  ASSERT_TRUE(WaveformBuilder::IsValidBlob(good));

  // Empty and too-short blobs.
  EXPECT_FALSE(WaveformBuilder::IsValidBlob(QByteArray()));
  EXPECT_FALSE(WaveformBuilder::IsValidBlob(good.left(8)));

  // Bad magic.
  QByteArray bad_magic = good;
  bad_magic[0] = 'X';
  EXPECT_FALSE(WaveformBuilder::IsValidBlob(bad_magic));

  // Unknown (future) version.
  QByteArray bad_version = good;
  bad_version[4] = static_cast<char>(WaveformBuilder::kWaveformVersion + 1);
  EXPECT_FALSE(WaveformBuilder::IsValidBlob(bad_version));

  // Truncated body (declared count no longer matches the actual length).
  EXPECT_FALSE(WaveformBuilder::IsValidBlob(good.left(good.size() - 10)));

}
