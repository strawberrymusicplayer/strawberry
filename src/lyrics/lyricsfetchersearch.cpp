/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QTimer>
#include <QList>
#include <QtDebug>

#include "core/logging.h"
#include "lyricsfetcher.h"
#include "lyricsfetchersearch.h"
#include "lyricsprovider.h"
#include "lyricsproviders.h"

const int LyricsFetcherSearch::kSearchTimeoutMs = 3000;
const int LyricsFetcherSearch::kGoodLyricsLength = 60;
const float LyricsFetcherSearch::kHighScore = 2.5;

LyricsFetcherSearch::LyricsFetcherSearch(const LyricsSearchRequest &request, QObject *parent)
    : QObject(parent),
      request_(request),
      cancel_requested_(false) {

  QTimer::singleShot(kSearchTimeoutMs, this, SLOT(TerminateSearch()));

}

void LyricsFetcherSearch::TerminateSearch() {

  for (int id : pending_requests_.keys()) {
    pending_requests_.take(id)->CancelSearch(id);
  }
  AllProvidersFinished();

}

void LyricsFetcherSearch::Start(LyricsProviders *lyrics_providers) {

  QList<LyricsProvider*> lyrics_providers_sorted = lyrics_providers->List();
  std::stable_sort(lyrics_providers_sorted.begin(), lyrics_providers_sorted.end(), ProviderCompareOrder);

  for (LyricsProvider *provider : lyrics_providers_sorted) {
    if (!provider->is_enabled() || !provider->IsAuthenticated()) continue;
    connect(provider, SIGNAL(SearchFinished(quint64, LyricsSearchResults)), SLOT(ProviderSearchFinished(quint64, LyricsSearchResults)));
    const int id = lyrics_providers->NextId();
    const bool success = provider->StartSearch(request_.artist, request_.album, request_.title, id);
    if (success) pending_requests_[id] = provider;
  }

  if (pending_requests_.isEmpty()) TerminateSearch();

}

void LyricsFetcherSearch::ProviderSearchFinished(const quint64 id, const LyricsSearchResults &results) {

  if (!pending_requests_.contains(id)) return;
  LyricsProvider *provider = pending_requests_.take(id);

  LyricsSearchResults results_copy(results);
  float higest_score = 0.0;
  for (int i = 0 ; i < results_copy.count() ; ++i) {
    results_copy[i].provider = provider->name();
    results_copy[i].score = 0.0;
    if (results_copy[i].artist.toLower() == request_.artist.toLower()) {
      results_copy[i].score += 0.5;
    }
    if (results_copy[i].album.toLower() == request_.album.toLower()) {
      results_copy[i].score += 0.5;
    }
    if (results_copy[i].title.toLower() == request_.title.toLower()) {
      results_copy[i].score += 0.5;
    }
    if (results_copy[i].artist.toLower() != request_.artist.toLower() && results_copy[i].title.toLower() != request_.title.toLower()) {
      results_copy[i].score -= 1.5;
    }
    if (results_copy[i].lyrics.length() > kGoodLyricsLength) results_copy[i].score += 1.0;
    if (results_copy[i].score > higest_score) higest_score = results_copy[i].score;
  }

  results_.append(results_copy);
  std::stable_sort(results_.begin(), results_.end(), LyricsSearchResultCompareScore);

  if (!pending_requests_.isEmpty()) {
    if (!results_.isEmpty() && higest_score >= kHighScore) { // Highest score, no need to wait for other providers.
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
    emit LyricsFetched(request_.id, results_.last().provider, results_.last().lyrics);
  }

  emit SearchFinished(request_.id, results_);

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

