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

// Reads back the versioned header (magic, version, count, peak) from a blob,
// leaving the stream positioned at the start of the int8 body.
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
  const QByteArray data = builder.Finish(kWaveformBaseCount);
  EXPECT_TRUE(data.isEmpty());

}

TEST(WaveformBuilderTest, HeaderLayoutAndBodyLength) {

  WaveformBuilder builder;
  std::vector<qint16> samples(8192, 0);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] = static_cast<qint16>((i % 2 == 0) ? 12000 : -8000);
  }
  builder.AddSamples(samples.data(), static_cast<int>(samples.size()));

  const QByteArray data = builder.Finish(kWaveformBaseCount);

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
  EXPECT_EQ(count, static_cast<quint32>(kWaveformBaseCount));
  EXPECT_GT(peak, 0.0f);

  // Header = 4 (magic) + 1 (version) + 4 (count) + 4 (peak) = 13 bytes;
  // body = count * 2 (one qint8 min + one qint8 max per bucket).
  const int header_bytes = 4 + 1 + 4 + 4;
  EXPECT_EQ(data.size(), header_bytes + kWaveformBaseCount * 2);

}

TEST(WaveformBuilderTest, NeverAveragesPreservingExtremes) {

  // A single bucket sub-range containing both a deep negative trough and a high
  // positive peak must retain both extremes, not average them toward zero.
  WaveformBuilder builder;
  std::vector<qint16> samples;
  samples.reserve(kWaveformBaseCount * 4);
  for (int i = 0; i < kWaveformBaseCount * 4; ++i) {
    samples.push_back(static_cast<qint16>((i % 2 == 0) ? 30000 : -30000));
  }
  builder.AddSamples(samples.data(), static_cast<int>(samples.size()));

  const QByteArray data = builder.Finish(kWaveformBaseCount);

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
