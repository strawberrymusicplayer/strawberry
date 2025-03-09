/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>

#include <QTimer>
#include <QList>

#include "core/logging.h"
#include "includes/shared_ptr.h"
#include "lyricsfetchersearch.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"
#include "lyricsprovider.h"
#include "lyricsproviders.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kSearchTimeoutMs = 3000;
constexpr int kGoodLyricsLength = 60;
constexpr float kHighScore = 2.5;
}  // namespace

LyricsFetcherSearch::LyricsFetcherSearch(const quint64 id, const LyricsSearchRequest &request, QObject *parent)
    : QObject(parent),
      id_(id),
      request_(request),
      cancel_requested_(false) {

  QTimer::singleShot(kSearchTimeoutMs, this, &LyricsFetcherSearch::TerminateSearch);

}

void LyricsFetcherSearch::TerminateSearch() {

  const QList<int> keys = pending_requests_.keys();
  for (const int id : keys) {
    pending_requests_.take(id)->CancelSearchAsync(id);
  }
  AllProvidersFinished();

}

void LyricsFetcherSearch::Start(SharedPtr<LyricsProviders> lyrics_providers) {

  // Ignore Radio Paradise "commercial" break.
  if (request_.artist.compare("commercial-free"_L1, Qt::CaseInsensitive) == 0 && request_.title.compare("listener-supported"_L1, Qt::CaseInsensitive) == 0) {
    TerminateSearch();
    return;
  }

  QList<LyricsProvider*> lyrics_providers_sorted = lyrics_providers->List();
  std::stable_sort(lyrics_providers_sorted.begin(), lyrics_providers_sorted.end(), ProviderCompareOrder);

  for (LyricsProvider *provider : std::as_const(lyrics_providers_sorted)) {
    if (!provider->is_enabled() || (provider->authentication_required() && !provider->authenticated())) continue;
    QObject::connect(provider, &LyricsProvider::SearchFinished, this, &LyricsFetcherSearch::ProviderSearchFinished);
    const int id = lyrics_providers->NextId();
    const bool success = provider->StartSearchAsync(id, request_);
    if (success) {
      pending_requests_.insert(id, provider);
    }
  }

  if (pending_requests_.isEmpty()) TerminateSearch();

}

void LyricsFetcherSearch::ProviderSearchFinished(const int id, const LyricsSearchResults &results) {

  if (!pending_requests_.contains(id)) return;
  LyricsProvider *provider = pending_requests_.take(id);

  LyricsSearchResults results_copy(results);
  float higest_score = 0.0;
  for (int i = 0; i < results_copy.count(); ++i) {
    results_copy[i].provider = provider->name();
    results_copy[i].score = 0.0;
    if (results_copy[i].artist.compare(request_.albumartist, Qt::CaseInsensitive) == 0 || results_copy[i].artist.compare(request_.artist, Qt::CaseInsensitive) == 0) {
      results_copy[i].score += 0.5;
    }
    if (results_copy[i].album.compare(request_.album, Qt::CaseInsensitive) == 0) {
      results_copy[i].score += 0.5;
    }
    if (results_copy[i].title.compare(request_.title, Qt::CaseInsensitive) == 0) {
      results_copy[i].score += 0.5;
    }
    if (results_copy[i].artist.compare(request_.artist, Qt::CaseInsensitive) != 0 && results_copy[i].title.compare(request_.title, Qt::CaseInsensitive) != 0) {
      results_copy[i].score -= 1.5;
    }
    if (results_copy[i].lyrics.length() > kGoodLyricsLength) results_copy[i].score += 1.0;
    if (results_copy[i].score > higest_score) higest_score = results_copy[i].score;
  }

  results_.append(results_copy);
  std::stable_sort(results_.begin(), results_.end(), LyricsSearchResultCompareScore);

  if (!pending_requests_.isEmpty()) {
    if (!results_.isEmpty() && higest_score >= kHighScore) {  // Highest score, no need to wait for other providers.
      qLog(Debug) << "Got lyrics with high score from" << results_.last().provider << "for" << request_.artist << request_.title << "score" << results_.last().score << "finishing search.";
      TerminateSearch();
    }
    else {
      return;
    }
    return;
  }

  AllProvidersFinished();

}

void LyricsFetcherSearch::AllProvidersFinished() {

  if (cancel_requested_) return;

  if (!results_.isEmpty()) {
    qLog(Debug) << "Using lyrics from" << results_.last().provider << "for" << request_.artist << request_.title << "with score" << results_.last().score;
    Q_EMIT LyricsFetched(id_, results_.constLast().provider, results_.constLast().lyrics);
  }
  else {
    Q_EMIT LyricsFetched(id_, QString(), QString());
  }

  Q_EMIT SearchFinished(id_, results_);

}

void LyricsFetcherSearch::Cancel() {

  cancel_requested_ = true;

  if (!pending_requests_.isEmpty()) {
    TerminateSearch();
  }

}

bool LyricsFetcherSearch::ProviderCompareOrder(LyricsProvider *a, LyricsProvider *b) {
  return a->order() < b->order();
}

bool LyricsFetcherSearch::LyricsSearchResultCompareScore(const LyricsSearchResult &a, const LyricsSearchResult &b) {
  return a.score < b.score;
}
