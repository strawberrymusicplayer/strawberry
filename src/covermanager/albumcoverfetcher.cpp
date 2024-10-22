/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QObject>
#include <QTimer>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "albumcoverfetcher.h"
#include "albumcoverfetchersearch.h"

using namespace std::chrono_literals;

namespace {
constexpr int kMaxConcurrentRequests = 5;
}

AlbumCoverFetcher::AlbumCoverFetcher(SharedPtr<CoverProviders> cover_providers, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      cover_providers_(cover_providers),
      network_(network),
      next_id_(0),
      request_starter_(new QTimer(this)) {

  request_starter_->setInterval(1s);
  QObject::connect(request_starter_, &QTimer::timeout, this, &AlbumCoverFetcher::StartRequests);

}

AlbumCoverFetcher::~AlbumCoverFetcher() {

  const QList<AlbumCoverFetcherSearch*> searches = active_requests_.values();
  for (AlbumCoverFetcherSearch *search : searches) {
    search->disconnect();
    search->deleteLater();
  }
  active_requests_.clear();

}

quint64 AlbumCoverFetcher::FetchAlbumCover(const QString &artist, const QString &album, const QString &title, const bool batch) {

  CoverSearchRequest request;
  request.id = ++next_id_;
  request.artist = artist;
  request.album = Song::AlbumRemoveDiscMisc(album);
  request.title = title;
  request.search = false;
  request.batch = batch;

  AddRequest(request);
  return request.id;

}

quint64 AlbumCoverFetcher::SearchForCovers(const QString &artist, const QString &album, const QString &title) {

  CoverSearchRequest request;
  request.id = ++next_id_;
  request.artist = artist;
  request.album = Song::AlbumRemoveDiscMisc(album);
  request.title = title;
  request.search = true;
  request.batch = false;

  AddRequest(request);
  return request.id;

}

void AlbumCoverFetcher::AddRequest(const CoverSearchRequest &req) {

  queued_requests_.enqueue(req);

  if (!request_starter_->isActive()) request_starter_->start();

  if (active_requests_.size() < kMaxConcurrentRequests) StartRequests();

}

void AlbumCoverFetcher::Clear() {

  queued_requests_.clear();

  const QList<AlbumCoverFetcherSearch*> searches = active_requests_.values();
  for (AlbumCoverFetcherSearch *search : searches) {
    search->Cancel();
    search->deleteLater();
  }
  active_requests_.clear();

}

void AlbumCoverFetcher::StartRequests() {

  if (queued_requests_.isEmpty()) {
    request_starter_->stop();
    return;
  }

  while (!queued_requests_.isEmpty() && active_requests_.size() < kMaxConcurrentRequests) {

    CoverSearchRequest request = queued_requests_.dequeue();

    // Search objects are this fetcher's children so worst case scenario - they get deleted with it
    AlbumCoverFetcherSearch *search = new AlbumCoverFetcherSearch(request, network_, this);
    active_requests_.insert(request.id, search);

    QObject::connect(search, &AlbumCoverFetcherSearch::SearchFinished, this, &AlbumCoverFetcher::SingleSearchFinished);
    QObject::connect(search, &AlbumCoverFetcherSearch::AlbumCoverFetched, this, &AlbumCoverFetcher::SingleCoverFetched);

    search->Start(cover_providers_);
  }

}

void AlbumCoverFetcher::SingleSearchFinished(const quint64 request_id, const CoverProviderSearchResults &results) {

  if (!active_requests_.contains(request_id)) return;
  AlbumCoverFetcherSearch *search = active_requests_.take(request_id);

  search->deleteLater();
  Q_EMIT SearchFinished(request_id, results, search->statistics());

}

void AlbumCoverFetcher::SingleCoverFetched(const quint64 request_id, const AlbumCoverImageResult &result) {

  if (!active_requests_.contains(request_id)) return;
  AlbumCoverFetcherSearch *search = active_requests_.take(request_id);

  search->deleteLater();
  Q_EMIT AlbumCoverFetched(request_id, result, search->statistics());

}
