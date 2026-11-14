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

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QEventLoop>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/song.h"
#include "collection/collectionmanager.h"
#include "collection/collectionbackend.h"
#include "playlist/playlist.h"
#include "playlist/playlistview.h"
#include "smartplaylists/smartplaylistsearch.h"
#include "smartplaylists/smartplaylistsearchterm.h"
#include "smartplaylists/smartplaylistsearchpreview.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// At file scope, since it's a friend of SmartPlaylistSearchPreview.
class SmartPlaylistSearchPreviewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    // A database file, since the searches run in another thread, which has its own connection, and a memory database is not shared between connections.
    database_ = make_shared<Database>(nullptr, nullptr, temp_dir_.filePath(u"strawberry.db"_s));
    backend_ = make_shared<CollectionBackend>();
    backend_->Init(database_, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    backend_->AddDirectory(u"/music"_s);
    AddSong(u"A"_s);
    AddSong(u"B"_s);

    preview_ = std::make_unique<SmartPlaylistSearchPreview>();
    // Like Init(), without initializing the playlist view, which needs the player.
    preview_->collection_backend_ = backend_;
    preview_->model_ = new Playlist(nullptr, nullptr, nullptr, backend_, nullptr, -1, QString(), false, &*preview_);
    // The playlist view is not initialized, so it's hidden, the results are checked in the model.
    PlaylistView *tree = preview_->findChild<PlaylistView*>(u"tree"_s);
    ASSERT_NE(nullptr, tree);
    tree->hide();
    preview_->show();
  }

  void TearDown() override {
    WaitForSearches();
    preview_.reset();
    backend_.reset();
    if (database_) {
      database_->Close();
      database_.reset();
    }
  }

  void AddSong(const QString &title) {
    Song song(Song::Source::Collection);
    song.Init(title, u"Artist"_s, u"Album"_s, 180 * 1000000000LL);
    song.set_directory_id(1);
    song.set_mtime(1);
    song.set_ctime(1);
    song.set_filesize(1);
    song.set_url(QUrl::fromLocalFile(u"/music/"_s + title + u".flac"_s));
    backend_->AddOrUpdateSongs(SongList() << song);
  }

  static SmartPlaylistSearch TitleSearch(const QString &title) {
    return SmartPlaylistSearch(SmartPlaylistSearch::SearchType::And, SmartPlaylistSearch::TermList() << SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Title, SmartPlaylistSearchTerm::Operator::Equals, title), SmartPlaylistSearch::SortType::FieldAsc, SmartPlaylistSearchTerm::Field::Title, -1);
  }

  // Waits until no search is running, also the searches started when a search finished.
  bool WaitForSearches(const int timeout_ms = 10000) const {
    QElapsedTimer timer;
    timer.start();
    while (preview_ && preview_->generator_) {
      if (timer.elapsed() > timeout_ms) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return true;
  }

  QStringList ShownTitles() const {
    QStringList titles;
    for (int row = 0; row < preview_->model_->rowCount(); ++row) {
      titles << preview_->model_->data(preview_->model_->index(row, static_cast<int>(Playlist::Column::Title))).toString();
    }
    return titles;
  }

  QTemporaryDir temp_dir_;
  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> backend_;
  std::unique_ptr<SmartPlaylistSearchPreview> preview_;
};

namespace {

// Regression test: changing the search back to the search shown while the changed search was running kept the results of the changed search, since the search shown was not run again.
TEST_F(SmartPlaylistSearchPreviewTest, ChangingBackWhileSearchingShowsResultsOfSearch) {

  preview_->Update(TitleSearch(u"A"_s));
  ASSERT_TRUE(WaitForSearches());
  ASSERT_EQ(QStringList() << u"A"_s, ShownTitles());

  // The search is changed, and changed back before it finishes.
  preview_->Update(TitleSearch(u"B"_s));
  preview_->Update(TitleSearch(u"A"_s));
  ASSERT_TRUE(WaitForSearches());

  EXPECT_EQ(QStringList() << u"A"_s, ShownTitles());

}

// Regression test: a search without terms, like when the last term was removed, was not run when it was changed while another search was running.
TEST_F(SmartPlaylistSearchPreviewTest, RemovingLastTermWhileSearchingShowsAllSongs) {

  preview_->Update(TitleSearch(u"A"_s));
  ASSERT_TRUE(WaitForSearches());
  ASSERT_EQ(QStringList() << u"A"_s, ShownTitles());

  preview_->Update(TitleSearch(u"B"_s));
  preview_->Update(SmartPlaylistSearch(SmartPlaylistSearch::SearchType::And, SmartPlaylistSearch::TermList(), SmartPlaylistSearch::SortType::FieldAsc, SmartPlaylistSearchTerm::Field::Title, -1));
  ASSERT_TRUE(WaitForSearches());

  EXPECT_EQ(QStringList() << u"A"_s << u"B"_s, ShownTitles());

}

// A search changed while the widget is hidden is run when it's shown.
TEST_F(SmartPlaylistSearchPreviewTest, SearchChangedWhileHiddenIsRunWhenShown) {

  preview_->Update(TitleSearch(u"A"_s));
  ASSERT_TRUE(WaitForSearches());

  preview_->hide();
  preview_->Update(TitleSearch(u"B"_s));
  ASSERT_TRUE(WaitForSearches());
  EXPECT_EQ(QStringList() << u"A"_s, ShownTitles());

  preview_->show();
  ASSERT_TRUE(WaitForSearches());
  EXPECT_EQ(QStringList() << u"B"_s, ShownTitles());

}

}  // namespace
