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

#include "config.h"

#include "gtest_include.h"
#include "gmock_include.h"

#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QString>
#include <QUrl>
#include <QNetworkDiskCache>
#include <QNetworkCacheMetaData>

#include "engine/gststartup.h"
#include "core/settings.h"
#include "core/standardpaths.h"
#include "constants/waveformsettings.h"
#include "waveform/waveformbuilder.h"
#include "waveform/waveformloader.h"
#include "waveform/waveformpipeline.h"

#include "test_utils.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Drives the real async loader against the bundled WAV from an isolated empty cache (QStandardPaths test mode), covering in-flight URL dedupe (EXT-03) and cache reuse on replay with no second decode (STO-03).
class WaveformLoaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    GstStartup::Initialize();

    // Isolate the cache: redirect CacheLocation to a per-user test path BEFORE constructing the loader (the loader fixes its cache dir in its ctor).
    QStandardPaths::setTestModeEnabled(true);

    // Guarantee a miss on the first Load even across repeated test runs.
    const QString waveform_cache_dir = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + u"/waveform"_s;
    QDir(waveform_cache_dir).removeRecursively();
  }

  // Writes a raw blob into the same on-disk cache the loader reads from, keyed exactly as WaveformLoader does, so Load() will find it as a cache "hit".
  static void SeedCache(const QString &filename, const QByteArray &blob) {
    const QString waveform_cache_dir = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + u"/waveform"_s;
    QNetworkDiskCache cache;
    cache.setCacheDirectory(waveform_cache_dir);
    QNetworkCacheMetaData metadata;
    metadata.setSaveToDisk(true);
    metadata.setUrl(QUrl(QString::fromLatin1(QUrl::toPercentEncoding(filename))));
    metadata.setRawHeaders(QNetworkCacheMetaData::RawHeaderList() << qMakePair(QByteArray("waveform"), QByteArray("waveform")));
    QIODevice *device = cache.prepare(metadata);
    ASSERT_TRUE(device);
    device->write(blob);
    cache.insert(device);
  }
};

TEST_F(WaveformLoaderTest, DedupesInFlightAndReusesCacheOnReplay) {

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  WaveformLoader loader;

  // First Load on an empty cache must take the async generation path (EXT-03).
  WaveformLoader::LoadResult first = loader.Load(url, false);
  ASSERT_EQ(first.status, WaveformLoader::LoadStatus::WillLoadAsync);
  ASSERT_TRUE(first.pipeline);

  // A second Load while the first is in flight returns the SAME pipeline (EXT-03 in-flight URL dedupe).
  WaveformLoader::LoadResult duplicate = loader.Load(url, false);
  EXPECT_EQ(duplicate.status, WaveformLoader::LoadStatus::WillLoadAsync);
  EXPECT_EQ(duplicate.pipeline.data(), first.pipeline.data());

  // Wait for generation to complete and assert a successful decode, not just signal arrival.
  QSignalSpy spy(&*first.pipeline, &WaveformPipeline::Finished);
  ASSERT_TRUE(spy.wait(10000));
  EXPECT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.takeFirst().at(0).toBool());

  // A later Load returns Loaded synchronously from the cache - no second decode (STO-03 cache reuse).
  WaveformLoader::LoadResult replay = loader.Load(url, false);
  EXPECT_EQ(replay.status, WaveformLoader::LoadStatus::Loaded);
  EXPECT_GT(replay.data.size(), 16);
  EXPECT_EQ(replay.data.left(4), QByteArray("SWVF"));

}

TEST_F(WaveformLoaderTest, RegeneratesOnMalformedCacheBlob) {

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  // Seed the cache with a bad-magic blob.
  // The loader must reject it and take the async regeneration path instead of returning the garbage as Loaded.
  SeedCache(res.fileName(), QByteArrayLiteral("XXXX\x01____________"));

  WaveformLoader loader;
  WaveformLoader::LoadResult result = loader.Load(url, false);
  EXPECT_EQ(result.status, WaveformLoader::LoadStatus::WillLoadAsync);
  ASSERT_TRUE(result.pipeline);
  QSignalSpy spy_malformed(&*result.pipeline, &WaveformPipeline::Finished);
  ASSERT_TRUE(spy_malformed.wait(10000));
  EXPECT_TRUE(spy_malformed.takeFirst().at(0).toBool());

}

TEST_F(WaveformLoaderTest, RegeneratesOnTruncatedCacheBlob) {

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  // Valid header declaring a full count but with a truncated body.
  QByteArray blob;
  QDataStream stream(&blob, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
  stream.writeRawData("SWVF", 4);
  stream << static_cast<quint8>(1);
  stream << static_cast<quint32>(2000);  // declares 2000 buckets...
  stream << 1.0F;
  stream << static_cast<qint8>(10) << static_cast<qint8>(20);  // ...but only one pair.
  SeedCache(res.fileName(), blob);

  WaveformLoader loader;
  WaveformLoader::LoadResult result = loader.Load(url, false);
  EXPECT_EQ(result.status, WaveformLoader::LoadStatus::WillLoadAsync);
  ASSERT_TRUE(result.pipeline);
  QSignalSpy spy_truncated(&*result.pipeline, &WaveformPipeline::Finished);
  ASSERT_TRUE(spy_truncated.wait(10000));
  EXPECT_TRUE(spy_truncated.takeFirst().at(0).toBool());

}

TEST_F(WaveformLoaderTest, RegeneratesOnWrongVersionCacheBlob) {

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  // Valid magic and consistent length but an unknown (future) version byte.
  QByteArray blob;
  QDataStream stream(&blob, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
  stream.writeRawData("SWVF", 4);
  stream << static_cast<quint8>(99);  // unsupported version
  stream << static_cast<quint32>(1);
  stream << 1.0F;
  stream << static_cast<qint8>(10) << static_cast<qint8>(20);
  SeedCache(res.fileName(), blob);

  WaveformLoader loader;
  WaveformLoader::LoadResult result = loader.Load(url, false);
  EXPECT_EQ(result.status, WaveformLoader::LoadStatus::WillLoadAsync);
  ASSERT_TRUE(result.pipeline);
  QSignalSpy spy_wrong_version(&*result.pipeline, &WaveformPipeline::Finished);
  ASSERT_TRUE(spy_wrong_version.wait(10000));
  EXPECT_TRUE(spy_wrong_version.takeFirst().at(0).toBool());

}

// Builds a valid, fixed-resolution sidecar blob (WaveformBuilder::kWaveformBaseCount buckets) that can be distinguished from a pipeline-generated blob or a different cache blob by its sentinel content.
// The fixed resolution is what WaveformBuilder::IsValidBlob requires.
static QByteArray MakeMinimalValidBlob(qint8 sentinel_min = -10, qint8 sentinel_max = 10) {
  QByteArray blob;
  QDataStream stream(&blob, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
  stream.writeRawData(WaveformBuilder::kWaveformMagic, 4);
  stream << WaveformBuilder::kWaveformVersion;
  stream << static_cast<quint32>(WaveformBuilder::kWaveformBaseCount);
  stream << 1.0F;
  for (int i = 0; i < WaveformBuilder::kWaveformBaseCount; ++i) {
    stream << sentinel_min << sentinel_max;
  }
  return blob;
}

TEST_F(WaveformLoaderTest, SidecarPreferredOverCache) {

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  // Build two distinguishable valid blobs.
  const QByteArray sidecar_blob = MakeMinimalValidBlob(-10, 10);
  const QByteArray cache_blob = MakeMinimalValidBlob(-20, 20);
  ASSERT_NE(sidecar_blob, cache_blob);

  // Write the sidecar alongside the audio file (hidden variant, index 0).
  const QString sidecar_path = WaveformLoader::WaveformFilenames(res.fileName()).at(0);
  {
    QFile sf(sidecar_path);
    ASSERT_TRUE(sf.open(QIODevice::WriteOnly));
    sf.write(sidecar_blob);
    sf.close();
  }

  // Also seed the cache with different valid content.
  SeedCache(res.fileName(), cache_blob);

  WaveformLoader loader;
  WaveformLoader::LoadResult result = loader.Load(url, false);

  // Sidecar read must win over the cache.
  ASSERT_EQ(result.status, WaveformLoader::LoadStatus::Loaded);
  EXPECT_EQ(result.data, sidecar_blob);

  // Clean up sidecar file.
  QFile::remove(sidecar_path);

}

TEST_F(WaveformLoaderTest, ReloadSettingsReadsSave) {

  TemporaryResource res(u":/audio/strawberry.wav"_s);
  ASSERT_TRUE(res.open());
  const QUrl url = QUrl::fromLocalFile(res.fileName());

  // WaveformFilenames() is public static — derive the hidden sidecar path directly.
  const QString hidden_path = WaveformLoader::WaveformFilenames(res.fileName()).at(0);

  // NEGATIVE HALF: kSave=false (default) — no sidecar must be written after analysis.
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kSave, false);
    s.endGroup();
  }

  {
    WaveformLoader loader;  // constructor calls ReloadSettings() → save_=false
    WaveformLoader::LoadResult result = loader.Load(url, false);
    ASSERT_EQ(result.status, WaveformLoader::LoadStatus::WillLoadAsync);
    QSignalSpy spy(&*result.pipeline, &WaveformPipeline::Finished);
    ASSERT_TRUE(spy.wait(10000));
    EXPECT_TRUE(spy.takeFirst().at(0).toBool());
  }

  EXPECT_FALSE(QFile::exists(hidden_path));

  // POSITIVE HALF: set kSave=true, reload settings, clear cache, then re-analyze.
  // Clear the cache directory so Load() cannot return from the cache hit.
  const QString waveform_cache_dir = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + u"/waveform"_s;
  QDir(waveform_cache_dir).removeRecursively();

  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kSave, true);
    s.endGroup();
  }

  {
    WaveformLoader loader;  // ReloadSettings() in ctor sets save_=true
    WaveformLoader::LoadResult result2 = loader.Load(url, false);
    ASSERT_EQ(result2.status, WaveformLoader::LoadStatus::WillLoadAsync);
    QSignalSpy spy2(&*result2.pipeline, &WaveformPipeline::Finished);
    ASSERT_TRUE(spy2.wait(10000));
    EXPECT_TRUE(spy2.takeFirst().at(0).toBool());
  }

  EXPECT_TRUE(QFile::exists(hidden_path));

  // Cleanup.
  QFile::remove(hidden_path);
  {
    Settings s;
    s.beginGroup(WaveformSettings::kSettingsGroup);
    s.setValue(WaveformSettings::kSave, false);
    s.endGroup();
  }

}

}  // namespace
