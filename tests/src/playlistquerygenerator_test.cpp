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

#include <QString>
#include <QStringList>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
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

}  // namespace
