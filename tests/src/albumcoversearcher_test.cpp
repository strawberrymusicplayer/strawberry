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

#include <QList>
#include <QString>
#include <QIcon>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "widgets/searchfield.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverfetcher.h"
#include "covermanager/albumcoversearcher.h"
#include "covermanager/coverprovider.h"
#include "covermanager/coverproviders.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {

// A cover provider which records the searches, they are finished by the test.
class FakeCoverProvider : public CoverProvider {
 public:
  explicit FakeCoverProvider(const SharedPtr<NetworkAccessManager> network) : CoverProvider(u"Fake"_s, true, false, 1.0, true, true, network, nullptr) {}

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override {
    Q_UNUSED(artist);
    Q_UNUSED(album);
    Q_UNUSED(title);
    started_ids_ << id;
    return true;
  }

  void CancelSearch(const int id) override { cancelled_ids_ << id; }

  void FinishSearch(const int id) { Q_EMIT SearchFinished(id, CoverProviderSearchResults()); }

  QList<int> started_ids_;
  QList<int> cancelled_ids_;
};

class AlbumCoverSearcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network_ = make_shared<NetworkAccessManager>();
    cover_providers_ = make_shared<CoverProviders>();
    // The cover providers own the provider.
    provider_ = new FakeCoverProvider(network_);
    cover_providers_->AddProvider(provider_);
    fetcher_ = std::make_unique<AlbumCoverFetcher>(cover_providers_, network_);
    albumcover_loader_ = make_shared<AlbumCoverLoader>(nullptr);
  }

  SharedPtr<NetworkAccessManager> network_;
  SharedPtr<CoverProviders> cover_providers_;
  FakeCoverProvider *provider_ = nullptr;
  std::unique_ptr<AlbumCoverFetcher> fetcher_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
};

// Searches in the dialog like the user does.
void SearchInDialog(AlbumCoverSearcher *searcher, QPushButton **button_search) {
  SearchField *artist = searcher->findChild<SearchField*>(u"artist"_s);
  SearchField *album = searcher->findChild<SearchField*>(u"album"_s);
  *button_search = searcher->findChild<QPushButton*>(u"search"_s);
  ASSERT_NE(nullptr, artist);
  ASSERT_NE(nullptr, album);
  ASSERT_NE(nullptr, *button_search);
  artist->setText(u"Other artist"_s);
  album->setText(u"Other album"_s);
  (*button_search)->click();
}

// Regression test: Abort in the cover search dialog cancelled all the requests of the fetcher, also the automatic cover search for the playing song, which then never finished, so the cover kept showing the spinner.
TEST_F(AlbumCoverSearcherTest, AbortOnlyCancelsTheDialogSearch) {

  QSignalSpy fetched_spy(&*fetcher_, &AlbumCoverFetcher::AlbumCoverFetched);
  QSignalSpy search_finished_spy(&*fetcher_, &AlbumCoverFetcher::SearchFinished);

  // The automatic cover search for the playing song.
  const quint64 playing_request_id = fetcher_->FetchAlbumCover(u"Artist"_s, u"Album"_s, u"Title"_s, true);
  ASSERT_EQ(1, provider_->started_ids_.count());
  const int playing_provider_id = provider_->started_ids_.first();

  AlbumCoverSearcher searcher(QIcon(), albumcover_loader_, nullptr);
  searcher.Init(&*fetcher_);

  // Search, then abort the search.
  QPushButton *button_search = nullptr;
  SearchInDialog(&searcher, &button_search);
  if (HasFatalFailure()) return;
  ASSERT_EQ(2, provider_->started_ids_.count());
  const int dialog_provider_id = provider_->started_ids_.last();
  button_search->click();

  EXPECT_EQ(QList<int>() << dialog_provider_id, provider_->cancelled_ids_);

  // The aborted search is not reported.
  provider_->FinishSearch(dialog_provider_id);
  EXPECT_EQ(0, search_finished_spy.count());

  // The automatic cover search still finishes.
  provider_->FinishSearch(playing_provider_id);
  ASSERT_EQ(1, fetched_spy.count());
  EXPECT_EQ(playing_request_id, fetched_spy.first().first().toULongLong());

}

// When the fetcher already has the most requests running, the search in the dialog waits, an aborted search is not started later.
TEST_F(AlbumCoverSearcherTest, AbortedQueuedSearchIsNotStarted) {

  for (int i = 0; i < 5; ++i) {
    fetcher_->FetchAlbumCover(u"Artist"_s, u"Album %1"_s.arg(i), u"Title"_s, true);
  }
  ASSERT_EQ(5, provider_->started_ids_.count());

  AlbumCoverSearcher searcher(QIcon(), albumcover_loader_, nullptr);
  searcher.Init(&*fetcher_);

  QPushButton *button_search = nullptr;
  SearchInDialog(&searcher, &button_search);
  if (HasFatalFailure()) return;
  ASSERT_EQ(5, provider_->started_ids_.count());
  button_search->click();
  EXPECT_TRUE(provider_->cancelled_ids_.isEmpty());

  // A running request finishes, the queued requests are started by a timer.
  QSignalSpy fetched_spy(&*fetcher_, &AlbumCoverFetcher::AlbumCoverFetched);
  provider_->FinishSearch(provider_->started_ids_.first());
  ASSERT_EQ(1, fetched_spy.count());
  QTest::qWait(1500);

  EXPECT_EQ(5, provider_->started_ids_.count());

}

// Regression test: closing the dialog while a search was running left it running, so when the dialog was opened again, the new search aborted it instead of starting.
TEST_F(AlbumCoverSearcherTest, ReopeningDuringSearchStartsNewSearch) {

  AlbumCoverSearcher searcher(QIcon(), albumcover_loader_, nullptr);
  searcher.Init(&*fetcher_);

  // Opened and closed before the search finished.
  QTimer::singleShot(0, &searcher, &AlbumCoverSearcher::reject);
  searcher.Exec(u"Artist"_s, u"Album"_s);
  ASSERT_EQ(1, provider_->started_ids_.count());
  const int first_provider_id = provider_->started_ids_.first();
  EXPECT_EQ(QList<int>() << first_provider_id, provider_->cancelled_ids_);

  // Opened again.
  qsizetype started_when_open = 0;
  QTimer::singleShot(0, &searcher, [this, &searcher, &started_when_open]() {
    started_when_open = provider_->started_ids_.count();
    searcher.reject();
  });
  searcher.Exec(u"Other artist"_s, u"Other album"_s);
  EXPECT_EQ(2, started_when_open);

}

}  // namespace
