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

#include <memory>

#include "gtest_include.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QFuture>
#include <QThreadPool>
#include <QTemporaryDir>
#include <QtConcurrentRun>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/database.h"
#include "core/memorydatabase.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "playlist/playlistitem.h"
#include "smartplaylists/smartplaylistsearch.h"
#include "smartplaylists/smartplaylistsearchterm.h"
#include "smartplaylists/playlistquerygenerator.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

class PlaylistQueryGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = make_shared<MemoryDatabase>(nullptr);
    backend_ = make_shared<CollectionBackend>();
    backend_->Init(database_, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    backend_->AddDirectory(u"/tmp"_s);

    SongList songs;
    const QStringList titles = QStringList() << u"A"_s << u"B"_s << u"C"_s << u"D"_s << u"E"_s;
    for (const QString &title : titles) {
      Song song;
      song.set_directory_id(1);
      song.set_url(QUrl::fromLocalFile(u"/tmp/"_s + title + u".flac"_s));
      song.set_mtime(1);
      song.set_ctime(1);
      song.set_filesize(1);
      song.set_title(title);
      songs << song;
    }
    backend_->AddOrUpdateSongs(songs);
    ASSERT_EQ(5, backend_->GetAllSongs().count());
  }

  static QStringList Titles(const PlaylistItemPtrList &items) {
    QStringList titles;
    for (const PlaylistItemPtr &item : items) {
      titles << item->EffectiveMetadata().title();
    }
    return titles;
  }

  void AddSongs(const QStringList &titles) {
    SongList songs;
    for (const QString &title : titles) {
      Song song;
      song.set_directory_id(1);
      song.set_url(QUrl::fromLocalFile(u"/tmp/extra%1.flac"_s.arg(songs.count())));
      song.set_mtime(1);
      song.set_ctime(1);
      song.set_filesize(1);
      song.set_title(title);
      songs << song;
    }
    backend_->AddOrUpdateSongs(songs);
  }

  // Returns the sorted titles of the songs matching a single title term.
  QStringList TitlesMatching(const SmartPlaylistSearchTerm::Operator op, const QString &value) {
    const SmartPlaylistSearch search(SmartPlaylistSearch::SearchType::And, SmartPlaylistSearch::TermList() << SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Title, op, value), SmartPlaylistSearch::SortType::FieldAsc, SmartPlaylistSearchTerm::Field::Title, -1);
    PlaylistQueryGenerator generator(u"Test"_s, search, false);
    generator.set_collection_backend(backend_);
    return Titles(generator.Generate());
  }

  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> backend_;
};

TEST_F(PlaylistQueryGeneratorTest, SortedDynamicDoesNotSkipSongs) {

  const SmartPlaylistSearch search(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::FieldAsc, SmartPlaylistSearchTerm::Field::Title, 2);
  PlaylistQueryGenerator generator(u"Sorted"_s, search, true);
  generator.set_collection_backend(backend_);

  // Each batch should continue right after the previous one.
  QStringList titles = Titles(generator.Generate());
  titles << Titles(generator.GenerateMore(2));
  titles << Titles(generator.GenerateMore(2));

  EXPECT_EQ(QStringList() << u"A"_s << u"B"_s << u"C"_s << u"D"_s << u"E"_s, titles);

}

TEST_F(PlaylistQueryGeneratorTest, SortedDynamicDoesNotRepeatSongsWhenCollectionChanges) {

  const SmartPlaylistSearch search(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::FieldAsc, SmartPlaylistSearchTerm::Field::Title, 2);
  PlaylistQueryGenerator generator(u"Sorted"_s, search, true);
  generator.set_collection_backend(backend_);

  QStringList titles = Titles(generator.Generate());

  // A song added before the songs already added moves them one row down, the next batch should not add them again.
  Song song;
  song.set_directory_id(1);
  song.set_url(QUrl::fromLocalFile(u"/tmp/AA.flac"_s));
  song.set_mtime(1);
  song.set_ctime(1);
  song.set_filesize(1);
  song.set_title(u"AA"_s);
  backend_->AddOrUpdateSongs(SongList() << song);

  titles << Titles(generator.GenerateMore(2));
  titles << Titles(generator.GenerateMore(2));

  EXPECT_EQ(QStringList() << u"A"_s << u"B"_s << u"C"_s << u"D"_s << u"E"_s, titles);

}

TEST_F(PlaylistQueryGeneratorTest, RandomDynamicDoesNotRepeatSongs) {

  const SmartPlaylistSearch search(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::Random, SmartPlaylistSearchTerm::Field::Title, 2);
  PlaylistQueryGenerator generator(u"Random"_s, search, true);
  generator.set_collection_backend(backend_);

  QStringList titles = Titles(generator.Generate());
  titles << Titles(generator.GenerateMore(2));
  titles << Titles(generator.GenerateMore(2));

  // The songs already added are excluded, so all songs are added once.
  titles.sort();
  EXPECT_EQ(QStringList() << u"A"_s << u"B"_s << u"C"_s << u"D"_s << u"E"_s, titles);

}

TEST_F(PlaylistQueryGeneratorTest, TextTermsMatchWildcardsLiterally) {

  AddSongs(QStringList() << u"100% Hits"_s << u"100 Hits"_s << u"a_b"_s << u"axb"_s << u"back\\slash"_s);

  // "%" and "_" are LIKE wildcards, they should only match themselves.
  EXPECT_EQ(QStringList() << u"100% Hits"_s, TitlesMatching(SmartPlaylistSearchTerm::Operator::Contains, u"%"_s));
  EXPECT_EQ(QStringList() << u"a_b"_s, TitlesMatching(SmartPlaylistSearchTerm::Operator::Contains, u"_"_s));
  EXPECT_EQ(QStringList() << u"100% Hits"_s, TitlesMatching(SmartPlaylistSearchTerm::Operator::StartsWith, u"100%"_s));
  EXPECT_EQ(QStringList() << u"a_b"_s, TitlesMatching(SmartPlaylistSearchTerm::Operator::EndsWith, u"_b"_s));
  EXPECT_EQ(QStringList() << u"back\\slash"_s, TitlesMatching(SmartPlaylistSearchTerm::Operator::Contains, u"\\"_s));
  EXPECT_FALSE(TitlesMatching(SmartPlaylistSearchTerm::Operator::NotContains, u"_"_s).contains(u"a_b"_s));
  EXPECT_TRUE(TitlesMatching(SmartPlaylistSearchTerm::Operator::NotContains, u"_"_s).contains(u"axb"_s));

}

TEST_F(PlaylistQueryGeneratorTest, TextEqualsIsExactAndCaseInsensitive) {

  AddSongs(QStringList() << u"a_b"_s << u"axb"_s);

  // Equals should not treat "_" as a wildcard, but still ignore case like before.
  EXPECT_EQ(QStringList() << u"a_b"_s, TitlesMatching(SmartPlaylistSearchTerm::Operator::Equals, u"a_b"_s));
  EXPECT_EQ(QStringList() << u"a_b"_s, TitlesMatching(SmartPlaylistSearchTerm::Operator::Equals, u"A_B"_s));
  EXPECT_EQ(QStringList() << u"A"_s, TitlesMatching(SmartPlaylistSearchTerm::Operator::Equals, u"a"_s));

  // NotEquals is the opposite of Equals, so it ignores case too.
  EXPECT_FALSE(TitlesMatching(SmartPlaylistSearchTerm::Operator::NotEquals, u"a"_s).contains(u"A"_s));
  EXPECT_TRUE(TitlesMatching(SmartPlaylistSearchTerm::Operator::NotEquals, u"a"_s).contains(u"B"_s));

}

TEST(PlaylistQueryGeneratorConcurrentTest, ConcurrentRandomDynamicDoesNotRepeatSongs) {

  // A database file instead of a memory database, since every thread has its own connection, and a memory database is not shared between connections.
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  SharedPtr<Database> database = make_shared<Database>(nullptr, nullptr, temp_dir.filePath(u"strawberry.db"_s));
  SharedPtr<CollectionBackend> backend = make_shared<CollectionBackend>();
  backend->Init(database, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
  backend->AddDirectory(u"/tmp"_s);

  constexpr int kSongCount = 20;
  SongList songs;
  for (int i = 0; i < kSongCount; ++i) {
    Song song;
    song.set_directory_id(1);
    song.set_url(QUrl::fromLocalFile(u"/tmp/%1.flac"_s.arg(i)));
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_filesize(1);
    song.set_title(QString::number(i));
    songs << song;
  }
  backend->AddOrUpdateSongs(songs);
  ASSERT_EQ(kSongCount, backend->GetAllSongs().count());

  const SmartPlaylistSearch search(SmartPlaylistSearch::SearchType::All, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::Random, SmartPlaylistSearchTerm::Field::Title, kSongCount);
  PlaylistQueryGenerator generator(u"Random"_s, search, true);
  generator.set_collection_backend(backend);

  // Generate more songs from several threads at once, like a dynamic playlist advanced again before the previous songs were generated.
  // Each call excludes the songs added by the calls before it, so no song is added twice.
  constexpr int kCallCount = 10;
  QThreadPool threadpool;
  threadpool.setMaxThreadCount(kCallCount);
  QList<QFuture<PlaylistItemPtrList>> futures;
  for (int i = 0; i < kCallCount; ++i) {
    futures << QtConcurrent::run(&threadpool, [&generator]() { return generator.GenerateMore(1); });
  }

  QStringList titles;
  for (QFuture<PlaylistItemPtrList> &future : futures) {
    future.waitForFinished();
    for (const PlaylistItemPtr &item : future.result()) {
      titles << item->EffectiveMetadata().title();
    }
  }

  EXPECT_EQ(kCallCount, titles.count());
  EXPECT_EQ(kCallCount, QSet<QString>(titles.begin(), titles.end()).count());

}

}  // namespace
