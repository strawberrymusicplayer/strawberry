/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QList>
#include <QVariant>
#include <QSignalSpy>
#include <QTest>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "covermanager/albumcoverexporter.h"

namespace {

// Songs without a cover are skipped without reading the file, the tag reader is not used.
void AddExportRequests(AlbumCoverExporter *exporter, const int count) {

  for (int i = 0; i < count; ++i) {
    exporter->AddExportRequest(Song());
  }

}

QString Updates(const QSignalSpy &spy) {

  QStringList updates;
  for (const QList<QVariant> &arguments : spy) {
    updates << QStringLiteral("%1/%2/%3").arg(arguments.value(0).toInt()).arg(arguments.value(1).toInt()).arg(arguments.value(2).toInt());
  }
  return updates.join(QLatin1String(", "));

}

// Regression test: the covers being exported when aborting were still counted when they finished, for the next export, then it was reported as finished too early, and again for each cover after that.
TEST(AlbumCoverExporterTest, AbortedCoversAreNotCounted) {

  AlbumCoverExporter exporter{SharedPtr<TagReaderClient>()};
  QSignalSpy spy(&exporter, &AlbumCoverExporter::AlbumCoversExportUpdate);

  // The covers are received in the event loop, so the covers being exported when aborting are received after the next export starts.
  AddExportRequests(&exporter, 5);
  exporter.StartExporting();
  exporter.Cancel();

  AddExportRequests(&exporter, 2);
  exporter.StartExporting();

  ASSERT_TRUE(QTest::qWaitFor([&spy]() { return spy.count() >= 2; }, 5000)) << Updates(spy).toStdString();
  QTest::qWait(300);

  // Exported/skipped/all for each update.
  EXPECT_EQ("0/1/2, 0/2/2", Updates(spy).toStdString());

}

// An export without aborting reports each cover.
TEST(AlbumCoverExporterTest, ExportReportsEachCover) {

  AlbumCoverExporter exporter{SharedPtr<TagReaderClient>()};
  QSignalSpy spy(&exporter, &AlbumCoverExporter::AlbumCoversExportUpdate);

  AddExportRequests(&exporter, 7);
  exporter.StartExporting();

  ASSERT_TRUE(QTest::qWaitFor([&spy]() { return spy.count() >= 7; }, 5000)) << Updates(spy).toStdString();
  QTest::qWait(300);

  EXPECT_EQ("0/1/7, 0/2/7, 0/3/7, 0/4/7, 0/5/7, 0/6/7, 0/7/7", Updates(spy).toStdString());

}

}  // namespace
