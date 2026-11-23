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
      : FakeLyricsProvider(name, LyricsSearchResults() << result, delay_msec, parent) {}

  explicit FakeLyricsProvider(const QString &name, const LyricsSearchResults &results, const int delay_msec, QObject *parent = nullptr)
      : LyricsProvider(name, true, false, !results.isEmpty() && !results.first().synced_lyrics.isEmpty(), nullptr, parent), results_(results), delay_msec_(delay_msec), searches_(0) {}

  int searches() const { return searches_; }

 protected:
  void StartSearch(const int id, const LyricsSearchRequest &request) override {
    Q_UNUSED(request)
    ++searches_;
    QTimer::singleShot(delay_msec_, this, [this, id]() { Q_EMIT SearchFinished(id, results_); });
  }

 private:
  const LyricsSearchResults results_;
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

  // Lyrics matching the artist, album and title, too short to have the highest score, so the search waits for all providers.
  static LyricsSearchResult ShortResult(const QString &lyrics) {
    LyricsSearchResult result(lyrics);
    result.artist = u"Artist"_s;
    result.album = u"Album"_s;
    result.title = u"Title"_s;
    return result;
  }

  // Searches with two providers returning lyrics with the same score, the first provider in the order of the providers replies after the delay.
  static LyricsSearchResult SearchTiedLyrics(const int first_provider_delay_msec, const int second_provider_delay_msec) {
    SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
    lyrics_providers->AddProvider(new FakeLyricsProvider(u"First"_s, ShortResult(u"First lyrics"_s), first_provider_delay_msec, &*lyrics_providers));
    lyrics_providers->AddProvider(new FakeLyricsProvider(u"Second"_s, ShortResult(u"Second lyrics"_s), second_provider_delay_msec, &*lyrics_providers));
    LyricsFetcher fetcher(lyrics_providers);
    QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);
    fetcher.Search(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);
    if (!spy_fetched.wait(5000) || spy_fetched.count() != 1) return LyricsSearchResult();
    return spy_fetched.at(0).at(1).value<LyricsSearchResult>();
  }

  SharedPtr<LyricsProviders> lyrics_providers_;
};

// Regression test: of lyrics with the same score, the lyrics from the provider which replied last were used, instead of the lyrics from the first provider in the order of the providers.
TEST_F(LyricsFetcherTest, TiedLyricsUseProviderOrder) {

  const LyricsSearchResult result = SearchTiedLyrics(0, 200);
  EXPECT_EQ(u"First"_s, result.provider);
  EXPECT_EQ(u"First lyrics"_s, result.lyrics);

}

TEST_F(LyricsFetcherTest, TiedLyricsUseProviderOrderWhenFirstProviderRepliesLast) {

  const LyricsSearchResult result = SearchTiedLyrics(200, 0);
  EXPECT_EQ(u"First"_s, result.provider);
  EXPECT_EQ(u"First lyrics"_s, result.lyrics);

}

// Regression test: lyrics with the highest score finished the search, also when a provider before it in the order of the providers hadn't replied yet, so the order in which the providers replied still decided between lyrics with the highest score.
TEST_F(LyricsFetcherTest, HighScoreLyricsWaitForProviderBefore) {

  SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"First"_s, Result(u"First lyrics"_s), 200, &*lyrics_providers));
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Second"_s, Result(u"Second lyrics"_s), 0, &*lyrics_providers));
  LyricsFetcher fetcher(lyrics_providers);
  QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);
  fetcher.Search(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);
  ASSERT_TRUE(spy_fetched.wait(5000));
  ASSERT_EQ(1, spy_fetched.count());
  EXPECT_EQ(u"First"_s, spy_fetched.at(0).at(1).value<LyricsSearchResult>().provider);

}

// Regression test: the order of the pending providers was read from the providers, so changing the order in the settings during the search finished the search with lyrics which weren't the best in the order the lyrics are sorted by.
TEST_F(LyricsFetcherTest, HighScoreLyricsUseProviderOrderFromSearchStart) {

  SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
  FakeLyricsProvider *first_provider = new FakeLyricsProvider(u"First"_s, Result(u"First lyrics"_s), 200, &*lyrics_providers);
  lyrics_providers->AddProvider(first_provider);
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Second"_s, Result(u"Second lyrics"_s), 0, &*lyrics_providers));
  LyricsFetcher fetcher(lyrics_providers);
  QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);
  fetcher.Search(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);

  // The search starts from the event loop, the order is changed after it has started, before the second provider replies.
  QTimer::singleShot(0, first_provider, [first_provider]() { first_provider->set_order(100); });

  ASSERT_TRUE(spy_fetched.wait(5000));
  ASSERT_EQ(1, spy_fetched.count());
  EXPECT_EQ(u"First"_s, spy_fetched.at(0).at(1).value<LyricsSearchResult>().provider);

}

// Lyrics with the highest score finish the search when the providers before it have replied, without waiting for the providers after it.
TEST_F(LyricsFetcherTest, HighScoreLyricsDontWaitForProviderAfter) {

  SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"First"_s, ShortResult(u"First lyrics"_s), 100, &*lyrics_providers));
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Second"_s, Result(u"Second lyrics"_s), 0, &*lyrics_providers));
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Third"_s, Result(u"Third lyrics"_s), 3000, &*lyrics_providers));
  LyricsFetcher fetcher(lyrics_providers);
  QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);
  fetcher.Search(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);
  ASSERT_TRUE(spy_fetched.wait(1500));
  ASSERT_EQ(1, spy_fetched.count());
  EXPECT_EQ(u"Second"_s, spy_fetched.at(0).at(1).value<LyricsSearchResult>().provider);

}

// Lyrics without the highest score don't finish the search, also when they're from the first provider, since the providers after it can have better lyrics.
TEST_F(LyricsFetcherTest, LowerScoreLyricsWaitForProviderAfter) {

  SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"First"_s, ShortResult(u"First lyrics"_s), 0, &*lyrics_providers));
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Second"_s, Result(u"Second lyrics"_s), 200, &*lyrics_providers));
  LyricsFetcher fetcher(lyrics_providers);
  QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);
  fetcher.Search(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);
  ASSERT_TRUE(spy_fetched.wait(5000));
  ASSERT_EQ(1, spy_fetched.count());
  EXPECT_EQ(u"Second"_s, spy_fetched.at(0).at(1).value<LyricsSearchResult>().provider);

}

// Of lyrics with the same score from one provider, the provider's first lyrics are used.
TEST_F(LyricsFetcherTest, TiedLyricsFromOneProviderUseFirstLyrics) {

  SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Provider"_s, LyricsSearchResults() << ShortResult(u"One"_s) << ShortResult(u"Two"_s), 0, &*lyrics_providers));
  LyricsFetcher fetcher(lyrics_providers);
  QSignalSpy spy_fetched(&fetcher, &LyricsFetcher::LyricsFetched);
  fetcher.Search(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);
  ASSERT_TRUE(spy_fetched.wait(5000));
  ASSERT_EQ(1, spy_fetched.count());
  EXPECT_EQ(u"One"_s, spy_fetched.at(0).at(1).value<LyricsSearchResult>().lyrics);

}

// SearchForLyrics() returns the best lyrics first, and lyrics with the same score in the order of the providers.
TEST_F(LyricsFetcherTest, SearchForLyricsReturnsBestLyricsFirst) {

  SharedPtr<LyricsProviders> lyrics_providers = make_shared<LyricsProviders>();
  LyricsSearchResult worse_result = ShortResult(u"Worse lyrics"_s);
  worse_result.album = u"Other album"_s;
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Worse"_s, worse_result, 0, &*lyrics_providers));
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"First"_s, ShortResult(u"First lyrics"_s), 200, &*lyrics_providers));
  lyrics_providers->AddProvider(new FakeLyricsProvider(u"Second"_s, ShortResult(u"Second lyrics"_s), 0, &*lyrics_providers));
  LyricsFetcher fetcher(lyrics_providers);
  QSignalSpy spy_search_finished(&fetcher, &LyricsFetcher::SearchFinished);
  fetcher.SearchForLyrics(u"Artist"_s, u"Artist"_s, u"Album"_s, u"Title"_s, 100);
  ASSERT_TRUE(spy_search_finished.wait(5000));
  ASSERT_EQ(1, spy_search_finished.count());

  QStringList providers;
  const LyricsSearchResults results = spy_search_finished.at(0).at(1).value<LyricsSearchResults>();
  for (const LyricsSearchResult &result : results) {
    providers << result.provider;
  }
  EXPECT_EQ(QStringList() << u"First"_s << u"Second"_s << u"Worse"_s, providers);

}

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
