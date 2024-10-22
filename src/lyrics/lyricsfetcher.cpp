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

#include <chrono>

#include <QtGlobal>
#include <QTimer>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "lyricsfetcher.h"
#include "lyricsfetchersearch.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

using namespace std::chrono_literals;

namespace {
constexpr int kMaxConcurrentRequests = 5;
}

LyricsFetcher::LyricsFetcher(const SharedPtr<LyricsProviders> lyrics_providers, QObject *parent)
    : QObject(parent),
      lyrics_providers_(lyrics_providers),
      next_id_(0),
      request_starter_(new QTimer(this)) {

  request_starter_->setInterval(500ms);
  QObject::connect(request_starter_, &QTimer::timeout, this, &LyricsFetcher::StartRequests);

}

quint64 LyricsFetcher::Search(const QString &effective_albumartist, const QString &artist, const QString &album, const QString &title) {

  LyricsSearchRequest search_request;
  search_request.albumartist = effective_albumartist;
  search_request.artist = artist;
  search_request.album = Song::AlbumRemoveDiscMisc(album);
  search_request.title = Song::TitleRemoveMisc(title);

  Request request;
  request.id = ++next_id_;
  request.search_request = search_request;
  AddRequest(request);

  return request.id;

}

void LyricsFetcher::AddRequest(const Request &request) {

  queued_requests_.enqueue(request);

  if (!request_starter_->isActive()) request_starter_->start();

  if (active_requests_.size() < kMaxConcurrentRequests) StartRequests();

}

void LyricsFetcher::Clear() {

  queued_requests_.clear();

  const QList<LyricsFetcherSearch*> searches = active_requests_.values();
  for (LyricsFetcherSearch *search : searches) {
    search->Cancel();
    search->deleteLater();
  }
  active_requests_.clear();

}

void LyricsFetcher::StartRequests() {

  if (queued_requests_.isEmpty()) {
    request_starter_->stop();
    return;
  }

  while (!queued_requests_.isEmpty() && active_requests_.size() < kMaxConcurrentRequests) {

    Request request = queued_requests_.dequeue();

    LyricsFetcherSearch *search = new LyricsFetcherSearch(request.id, request.search_request, this);
    active_requests_.insert(request.id, search);

    QObject::connect(search, &LyricsFetcherSearch::SearchFinished, this, &LyricsFetcher::SingleSearchFinished);
    QObject::connect(search, &LyricsFetcherSearch::LyricsFetched, this, &LyricsFetcher::SingleLyricsFetched);

    search->Start(lyrics_providers_);
  }

}

void LyricsFetcher::SingleSearchFinished(const quint64 request_id, const LyricsSearchResults &results) {

  if (!active_requests_.contains(request_id)) return;

  LyricsFetcherSearch *search = active_requests_.take(request_id);
  search->deleteLater();
  Q_EMIT SearchFinished(request_id, results);

}

void LyricsFetcher::SingleLyricsFetched(const quint64 request_id, const QString &provider, const QString &lyrics) {

  if (!active_requests_.contains(request_id)) return;

  LyricsFetcherSearch *search = active_requests_.take(request_id);
  search->deleteLater();
  Q_EMIT LyricsFetched(request_id, provider, lyrics);

}
