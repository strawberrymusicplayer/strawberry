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

#include <memory>

#include "gtest_include.h"

#include <QByteArray>
#include <QFile>
#include <QFuture>
#include <QIODevice>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrentRun>

#include "includes/shared_ptr.h"
#include "core/memorydatabase.h"
#include "core/songloader.h"
#include "core/urlhandlers.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// clazy:excludeall=non-pod-global-static

namespace {

// Playlists which are not in the collection are parsed in a concurrent thread, the errors of the parser should be returned when it's finished.
TEST(SongLoaderTest, ParserErrorsFromConcurrentThread) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"playlist.m3u"_s);
  {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    // A URL with a scheme the parser doesn't know how to handle.
    file.write(QByteArray("unknownscheme://example/song.mp3\n"));
    file.close();
  }

  SharedPtr<Database> database = make_shared<MemoryDatabase>(nullptr);
  SharedPtr<CollectionBackend> collection_backend = make_shared<CollectionBackend>();
  collection_backend->Init(database, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));

  SongLoader loader(make_shared<UrlHandlers>(), collection_backend, nullptr);
  ASSERT_EQ(SongLoader::Result::BlockingLoadRequired, loader.Load(QUrl::fromLocalFile(filename)));

  // Load and read the errors in a concurrent thread, like SongLoaderInserter does.
  // Use a separate thread pool and wait for it, waiting for the future can run the task in this thread instead.
  QThreadPool threadpool;
  QFuture<QStringList> future = QtConcurrent::run(&threadpool, [&loader]() {
    loader.LoadFilenamesBlocking();
    return loader.errors();
  });
  threadpool.waitForDone();

  const QStringList errors = future.result();
  ASSERT_EQ(1, errors.count());
  EXPECT_TRUE(errors.first().contains(u"unknownscheme://example/song.mp3"_s));

}

}  // namespace
