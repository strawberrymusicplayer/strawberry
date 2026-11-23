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

// Live integration tests for the lyrics providers.
// Each test runs a Pink Floyd search through one provider and asserts that returned lyrics contain a known opening-line phrase from the song.
// We try "Time" first (the canary track) and fall back to "Money" if Time fails,
// since some providers' slug routers collide on "Time" (e.g. letras.mus.br resolves /pink-floyd/time/ to a different track called "Biding My Time").
// A provider is considered working if either song returns matching lyrics.
//
// These tests hit the real network and are intended as a canary for detecting when a provider's endpoint, HTML structure, or response schema breaks.
// They are NOT part of the default `strawberry_tests` aggregate; build and invoke `lyrics_live_tests` explicitly.

#include "gtest_include.h"

#include <memory>
#include <vector>

#include <QObject>
#include <QThread>
#include <QString>
#include <QSignalSpy>
#include <QVariant>

#include "test_utils.h"

#include "core/networkaccessmanager.h"
#include "lyrics/lyricsprovider.h"
#include "lyrics/lyricssearchrequest.h"
#include "lyrics/lyricssearchresult.h"

#include "lyrics/azlyricscomlyricsprovider.h"
#include "lyrics/elyricsnetlyricsprovider.h"
#include "lyrics/letraslyricsprovider.h"
#include "lyrics/lrcliblyricsprovider.h"
#include "lyrics/ovhlyricsprovider.h"
#include "lyrics/songlyricscomlyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Generous timeout -- some HTML scrapers chain several requests.
constexpr int kNetworkTimeoutMs = 30000;

struct SongFixture {
  LyricsSearchRequest request;
  QString lyrics_marker;  // distinctive substring expected in returned lyrics
};

SongFixture TimeFixture() {
  SongFixture fixture;
  fixture.request.albumartist = u"Pink Floyd"_s;
  fixture.request.artist = u"Pink Floyd"_s;
  fixture.request.album = u"The Dark Side of the Moon"_s;
  fixture.request.title = u"Time"_s;
  fixture.request.duration = 413;
  fixture.lyrics_marker = u"Ticking away"_s;
  return fixture;
}

SongFixture MoneyFixture() {
  SongFixture fixture;
  fixture.request.albumartist = u"Pink Floyd"_s;
  fixture.request.artist = u"Pink Floyd"_s;
  fixture.request.album = u"The Dark Side of the Moon"_s;
  fixture.request.title = u"Money"_s;
  fixture.request.duration = 382;
  fixture.lyrics_marker = u"Grab that cash"_s;
  return fixture;
}

// Outcome of a single attempt: human-readable on failure, empty on success.
QString TryFetchLyrics(LyricsProvider *provider, const SongFixture &fixture) {

  QSignalSpy spy(provider, &LyricsProvider::SearchFinished);
  if (!spy.isValid()) return u"QSignalSpy not valid"_s;

  provider->StartSearchAsync(1, fixture.request);

  if (!spy.wait(kNetworkTimeoutMs)) {
    return QStringLiteral("No SearchFinished signal within %1ms for \"%2\"").arg(kNetworkTimeoutMs).arg(fixture.request.title);
  }
  if (spy.count() < 1) return u"No SearchFinished captured"_s;

  const LyricsSearchResults results = spy.at(0).at(1).value<LyricsSearchResults>();
  if (results.isEmpty()) {
    return QStringLiteral("Provider returned no results for \"%1\"").arg(fixture.request.title);
  }

  for (const LyricsSearchResult &result : results) {
    if (result.lyrics.contains(fixture.lyrics_marker, Qt::CaseInsensitive)) {
      return QString();  // success
    }
  }

  return QStringLiteral("No result contained \"%1\" for \"%2\" (got %3 result(s))").arg(fixture.lyrics_marker, fixture.request.title).arg(results.count());

}

template <typename ProviderT>
void RunProviderLiveTest() {

  qRegisterMetaType<LyricsSearchResults>("LyricsSearchResults");

  QThread worker;
  std::shared_ptr<NetworkAccessManager> network = std::make_shared<NetworkAccessManager>();
  network->moveToThread(&worker);
  ProviderT *provider = new ProviderT(network, nullptr);
  provider->moveToThread(&worker);
  worker.start();

  // Try Time first (the canonical canary), then Money. A provider passes if any fixture succeeds; we only fail if every attempt failed.
  const std::vector<SongFixture> fallbacks = {TimeFixture(), MoneyFixture()};

  QStringList failure_messages;
  bool succeeded = false;
  for (const SongFixture &fixture : fallbacks) {
    const QString error = TryFetchLyrics(provider, fixture);
    if (error.isEmpty()) {
      succeeded = true;
      break;
    }
    failure_messages << error;
  }

  // Tear down before asserting so the worker thread always stops.
  QMetaObject::invokeMethod(provider, "deleteLater", Qt::QueuedConnection);
  worker.quit();
  worker.wait();
  network.reset();

  EXPECT_TRUE(succeeded) << "All fallback songs failed: " << failure_messages.join(u" | "_s).toStdString();

}

}  // namespace

TEST(LyricsLive, LrcLib)       { RunProviderLiveTest<LrcLibLyricsProvider>(); }
TEST(LyricsLive, OVH)          { RunProviderLiveTest<OVHLyricsProvider>(); }
TEST(LyricsLive, AzLyrics)     { RunProviderLiveTest<AzLyricsComLyricsProvider>(); }
TEST(LyricsLive, ElyricsNet)   { RunProviderLiveTest<ElyricsNetLyricsProvider>(); }
TEST(LyricsLive, Letras)       { RunProviderLiveTest<LetrasLyricsProvider>(); }
TEST(LyricsLive, SongLyrics)   { RunProviderLiveTest<SongLyricsComLyricsProvider>(); }
