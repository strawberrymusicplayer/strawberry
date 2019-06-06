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
#include <cmath>

#include <QObject>
#include <QtAlgorithms>
#include <QTimer>
#include <QList>
#include <QtDebug>

#include "core/closure.h"
#include "core/logging.h"
#include "lyricsfetcher.h"
#include "lyricsfetchersearch.h"
#include "lyricsprovider.h"
#include "lyricsproviders.h"

const int LyricsFetcherSearch::kSearchTimeoutMs = 6000;

LyricsFetcherSearch::LyricsFetcherSearch(
    const LyricsSearchRequest &request, QObject *parent)
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

  for (LyricsProvider *provider : lyrics_providers->List()) {
    connect(provider, SIGNAL(SearchFinished(const quint64, QList<LyricsSearchResult>)), SLOT(ProviderSearchFinished(const quint64, QList<LyricsSearchResult>)));
    const int id = lyrics_providers->NextId();
    const bool success = provider->StartSearch(request_.artist, request_.album, request_.title, id);
    if (success) pending_requests_[id] = provider;
  }

  if (pending_requests_.isEmpty()) TerminateSearch();

}

void LyricsFetcherSearch::ProviderSearchFinished(const quint64 id, const QList<LyricsSearchResult> &results) {

  if (!pending_requests_.contains(id)) return;
  LyricsProvider *provider = pending_requests_.take(id);

  LyricsSearchResults results_copy(results);
  for (int i = 0; i < results_copy.count(); ++i) {
    results_copy[i].provider = provider->name();
  }

  results_.append(results_copy);

  if (!pending_requests_.isEmpty()) {
    return;
  }

  AllProvidersFinished();

}

void LyricsFetcherSearch::AllProvidersFinished() {

  if (cancel_requested_) return;

  if (!results_.isEmpty()) {
    LyricsSearchResult result_use;
    result_use.score = 0.0;
    for (LyricsSearchResult result : results_) {
      if (result_use.lyrics.isEmpty() || result.score > result_use.score) result_use = result;
    }
    qLog(Debug) << "Using lyrics from" << result_use.provider << "for" << request_.artist << request_.title << "with score" << result_use.score;
    emit LyricsFetched(request_.id, result_use.provider, result_use.lyrics);
  }
  emit SearchFinished(request_.id, results_);

}

void LyricsFetcherSearch::Cancel() {

  cancel_requested_ = true;

  if (!pending_requests_.isEmpty()) {
    TerminateSearch();
  }

}

