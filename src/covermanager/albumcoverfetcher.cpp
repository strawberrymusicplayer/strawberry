/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QTimer>
#include <QString>
#include <QImage>

#include "core/network.h"
#include "core/song.h"
#include "albumcoverfetcher.h"
#include "albumcoverfetchersearch.h"

const int AlbumCoverFetcher::kMaxConcurrentRequests = 5;

AlbumCoverFetcher::AlbumCoverFetcher(CoverProviders *cover_providers, QObject *parent, NetworkAccessManager *network)
    : QObject(parent),
      cover_providers_(cover_providers),
      network_(network ? network : new NetworkAccessManager(this)),
      next_id_(0),
      request_starter_(new QTimer(this)) {

  request_starter_->setInterval(1000);
  connect(request_starter_, SIGNAL(timeout()), SLOT(StartRequests()));

}

AlbumCoverFetcher::~AlbumCoverFetcher() {

  for (AlbumCoverFetcherSearch *search : active_requests_.values()) {
    search->disconnect();
    search->deleteLater();
  }
  active_requests_.clear();

}

quint64 AlbumCoverFetcher::FetchAlbumCover(const QString &artist, const QString &album, const QString &title, bool fetchall) {

  CoverSearchRequest request;
  request.id = next_id_++;
  request.artist = artist;
  request.album = album;
  request.album = request.album.remove(Song::kAlbumRemoveDisc);
  request.album = request.album.remove(Song::kAlbumRemoveMisc);
  request.title = title;
  request.search = false;
  request.fetchall = fetchall;

  AddRequest(request);
  return request.id;

}

quint64 AlbumCoverFetcher::SearchForCovers(const QString &artist, const QString &album, const QString &title) {

  CoverSearchRequest request;
  request.id = next_id_++;
  request.artist = artist;
  request.album = album;
  request.album = request.album.remove(Song::kAlbumRemoveDisc);
  request.album = request.album.remove(Song::kAlbumRemoveMisc);
  request.title = title;
  request.search = true;
  request.fetchall = false;

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

  for (AlbumCoverFetcherSearch *search : active_requests_.values()) {
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

    connect(search, SIGNAL(SearchFinished(quint64, CoverSearchResults)), SLOT(SingleSearchFinished(quint64, CoverSearchResults)));
    connect(search, SIGNAL(AlbumCoverFetched(quint64, QUrl, QImage)), SLOT(SingleCoverFetched(quint64, QUrl, QImage)));

    search->Start(cover_providers_);
  }

}

void AlbumCoverFetcher::SingleSearchFinished(const quint64 request_id, const CoverSearchResults results) {

  if (!active_requests_.contains(request_id)) return;
  AlbumCoverFetcherSearch *search = active_requests_.take(request_id);

  search->deleteLater();
  emit SearchFinished(request_id, results, search->statistics());

}

void AlbumCoverFetcher::SingleCoverFetched(const quint64 request_id, const QUrl &cover_url, const QImage &image) {

  if (!active_requests_.contains(request_id)) return;
  AlbumCoverFetcherSearch *search = active_requests_.take(request_id);

  search->deleteLater();
  emit AlbumCoverFetched(request_id, cover_url, image, search->statistics());

}

