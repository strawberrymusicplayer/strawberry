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

#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QSignalSpy>

#include "includes/shared_ptr.h"
#include "lyrics/lyricsprovider.h"
#include "lyrics/lyricsproviders.h"
#include "lyrics/lyricsfetcher.h"
#include "lyrics/lyricssearchrequest.h"
#include "lyrics/lyricssearchresult.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

// clazy:excludeall=non-pod-global-static

namespace {

// Returns the lyrics after the delay, without using the network.
// The provider has synchronized lyrics when the result has synchronized lyrics.
class FakeLyricsProvider : public LyricsProvider {
 public:
  explicit FakeLyricsProvider(const QString &name, const LyricsSearchResult &result, const int delay_msec, QObject *parent = nullptr)
      : LyricsProvider(name, true, false, !result.synced_lyrics.isEmpty(), nullptr, parent), result_(result), delay_msec_(delay_msec), searches_(0) {}

  int searches() const { return searches_; }

 protected:
  void StartSearch(const int id, const LyricsSearchRequest &request) override {
    Q_UNUSED(request)
    ++searches_;
    QTimer::singleShot(delay_msec_, this, [this, id]() { Q_EMIT SearchFinished(id, LyricsSearchResults() << result_); });
  }

 private:
  const LyricsSearchResult result_;
  const int delay_msec_;
  int searches_;
};

class LyricsFetcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    qRegisterMetaType<LyricsSearchRequest>("LyricsSearchRequest");
    qRegisterMetaType<LyricsSearchResult>("LyricsSearchResult");
    qRegisterMetaType<LyricsSearchResults>("LyricsSearchResults");
    lyrics_providers_ = make_shared<LyricsProviders>();
    // The first provider returns lyrics with the highest score right away, the second one returns other lyrics later.
    lyrics_providers_->AddProvider(new FakeLyricsProvider(u"Fast"_s, Result(u"Fast lyrics"_s), 0, &*lyrics_providers_));
    lyrics_providers_->AddProvider(new FakeLyricsProvider(u"Slow"_s, Result(u"Slow lyrics"_s), 300, &*lyrics_providers_));
  }

  // Lyrics matching the artist, album and title, long enough to have the highest score.
  static LyricsSearchResult Result(const QString &lyrics) {
    LyricsSearchResult result(lyrics + QString(100, u' ') + u'.');
    result.artist = u"Artist"_s;
    result.album = u"Album"_s;
    result.title = u"Title"_s;
    return result;
  }

  SharedPtr<LyricsProviders> lyrics_providers_;
};

// Search() returns only the best lyrics, and finishes when lyrics with the highest score are found.
TEST_F(LyricsFetcherTest, SearchReturnsBestLyrics) {

  LyricsFetcher fetcher(lyrics_providers_);
  QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);
  QSignalSpy spy_search_finished(&fetcher, &LyricsFetcher::SearchFinished);

  const quint64 id = fetcher.Search(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);
  ASSERT_TRUE(spy_fetched.wait(5000));

  ASSERT_EQ(1, spy_fetched.count());
  EXPECT_EQ(id, spy_fetched.at(0).at(0).value<quint64>());
  const LyricsSearchResult result = spy_fetched.at(0).at(1).value<LyricsSearchResult>();
  EXPECT_EQ(u"Fast"_s, result.provider);
  EXPECT_TRUE(result.lyrics.startsWith(u"Fast lyrics"_s));

  // Waiting for the slow provider doesn't emit anything more.
  spy_fetched.wait(500);
  EXPECT_EQ(1, spy_fetched.count());
  EXPECT_EQ(0, spy_search_finished.count());

}

// SearchForLyrics() waits for all providers and returns all lyrics, also when lyrics with the highest score are found first.
TEST_F(LyricsFetcherTest, SearchForLyricsReturnsAllLyrics) {

  LyricsFetcher fetcher(lyrics_providers_);
  QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);
  QSignalSpy spy_search_finished(&fetcher, &LyricsFetcher::SearchFinished);

  const quint64 id = fetcher.SearchForLyrics(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);
  ASSERT_TRUE(spy_search_finished.wait(5000));

  ASSERT_EQ(1, spy_search_finished.count());
  EXPECT_EQ(id, spy_search_finished.at(0).at(0).value<quint64>());
  const LyricsSearchResults results = spy_search_finished.at(0).at(1).value<LyricsSearchResults>();
  QStringList providers;
  for (const LyricsSearchResult &result : results) {
    providers << result.provider;
  }
  providers.sort();
  EXPECT_EQ(QStringList() << u"Fast"_s << u"Slow"_s, providers);
  EXPECT_EQ(0, spy_fetched.count());

}

// Search() with synced only uses lyrics with synchronized lyrics, also when other lyrics have the highest score.
TEST_F(LyricsFetcherTest, SearchSyncedReturnsBestSyncedLyrics) {

  // The slow provider is the only one with synchronized lyrics.
  LyricsSearchResult synced_result = Result(u"Synced lyrics"_s);
  synced_result.synced_lyrics = u"[00:01.00]Synced lyrics"_s;
  SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
  FakeLyricsProvider *fast_provider = new FakeLyricsProvider(u"Fast"_s, Result(u"Fast lyrics"_s), 0, &*lyrics_providers);
  lyrics_providers->AddProvider(fast_provider);
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Synced"_s, synced_result, 300, &*lyrics_providers));

  LyricsFetcher fetcher(lyrics_providers);
  QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);

  fetcher.Search(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100, true);
  ASSERT_TRUE(spy_fetched.wait(5000));

  ASSERT_EQ(1, spy_fetched.count());
  const LyricsSearchResult result = spy_fetched.at(0).at(1).value<LyricsSearchResult>();
  EXPECT_EQ(u"Synced"_s, result.provider);
  EXPECT_EQ(u"[00:01.00]Synced lyrics"_s, result.synced_lyrics);

  // The provider without synchronized lyrics is not searched.
  EXPECT_EQ(0, fast_provider->searches());

}

// SearchForLyrics() with synced only searches the providers with synchronized lyrics.
TEST_F(LyricsFetcherTest, SearchForSyncedLyricsOnlySearchesProvidersWithSyncedLyrics) {

  LyricsSearchResult synced_result = Result(u"Synced lyrics"_s);
  synced_result.synced_lyrics = u"[00:01.00]Synced lyrics"_s;
  SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
  FakeLyricsProvider *plain_provider = new FakeLyricsProvider(u"Plain"_s, Result(u"Plain lyrics"_s), 0, &*lyrics_providers);
  FakeLyricsProvider *synced_provider = new FakeLyricsProvider(u"Synced"_s, synced_result, 0, &*lyrics_providers);
  lyrics_providers->AddProvider(plain_provider);
  lyrics_providers->AddProvider(synced_provider);

  LyricsFetcher fetcher(lyrics_providers);
  QSignalSpy spy_search_finished(&fetcher, &LyricsFetcher::SearchFinished);

  fetcher.SearchForLyrics(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100, true);
  ASSERT_TRUE(spy_search_finished.wait(5000));

  const LyricsSearchResults results = spy_search_finished.at(0).at(1).value<LyricsSearchResults>();
  ASSERT_EQ(1, results.count());
  EXPECT_EQ(u"Synced"_s, results.first().provider);
  EXPECT_EQ(0, plain_provider->searches());
  EXPECT_EQ(1, synced_provider->searches());

}

}  // namespace
