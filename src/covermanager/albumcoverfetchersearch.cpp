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

#include <algorithm>
#include <cmath>

#include <QObject>
#include <QCoreApplication>
#include <QTimer>
#include <QList>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>

#include "core/logging.h"
#include "core/networktimeouts.h"
#include "albumcoverfetcher.h"
#include "albumcoverfetchersearch.h"
#include "coverprovider.h"
#include "coverproviders.h"

const int AlbumCoverFetcherSearch::kSearchTimeoutMs = 20000;
const int AlbumCoverFetcherSearch::kImageLoadTimeoutMs = 6000;
const int AlbumCoverFetcherSearch::kTargetSize = 500;
const float AlbumCoverFetcherSearch::kGoodScore = 4.0;

AlbumCoverFetcherSearch::AlbumCoverFetcherSearch(
    const CoverSearchRequest &request, QNetworkAccessManager *network, QObject *parent)
    : QObject(parent),
      request_(request),
      image_load_timeout_(new NetworkTimeouts(kImageLoadTimeoutMs, this)),
      network_(network),
      cancel_requested_(false) {

  // We will terminate the search after kSearchTimeoutMs milliseconds if we are not able to find all of the results before that point in time
  QTimer::singleShot(kSearchTimeoutMs, this, SLOT(TerminateSearch()));

}

AlbumCoverFetcherSearch::~AlbumCoverFetcherSearch() {
  pending_requests_.clear();
  Cancel();
}

void AlbumCoverFetcherSearch::TerminateSearch() {

  for (quint64 id : pending_requests_.keys()) {
    pending_requests_.take(id)->CancelSearch(id);
  }

  AllProvidersFinished();

}

void AlbumCoverFetcherSearch::Start(CoverProviders *cover_providers) {

  QList<CoverProvider*> cover_providers_sorted = cover_providers->List();
  std::stable_sort(cover_providers_sorted.begin(), cover_providers_sorted.end(), ProviderCompareOrder);

  for (CoverProvider *provider : cover_providers_sorted) {

    if (!provider->is_enabled()) continue;

    // Skip any provider that requires authentication but is not authenticated.
    if (provider->AuthenticationRequired() && !provider->IsAuthenticated()) {
      continue;
    }

    // Skip provider if it does not have fetchall set and we are doing fetchall - "Fetch Missing Covers".
    if (!provider->fetchall() && request_.fetchall) {
      continue;
    }

    // If album is missing, check if we can still use this provider by searching using artist + title.
    if (!provider->allow_missing_album() && request_.album.isEmpty()) {
      continue;
    }

    connect(provider, SIGNAL(SearchFinished(int, CoverSearchResults)), SLOT(ProviderSearchFinished(int, CoverSearchResults)));
    const int id = cover_providers->NextId();
    const bool success = provider->StartSearch(request_.artist, request_.album, request_.title, id);

    if (success) {
      pending_requests_[id] = provider;
      statistics_.network_requests_made_++;
    }
  }

  // End this search before it even began if there are no providers...
  if (pending_requests_.isEmpty()) {
    TerminateSearch();
  }

}

void AlbumCoverFetcherSearch::ProviderSearchFinished(const int id, const CoverSearchResults &results) {

  if (!pending_requests_.contains(id)) return;
  CoverProvider *provider = pending_requests_.take(id);

  CoverSearchResults results_copy(results);
  for (int i = 0 ; i < results_copy.count() ; ++i) {
    results_copy[i].provider = provider->name();
    results_copy[i].score = provider->quality();
    if (results_copy[i].artist.toLower() == request_.artist.toLower()) {
      results_copy[i].score += 0.5;
    }
    if (results_copy[i].album.toLower() == request_.album.toLower()) {
      results_copy[i].score += 0.5;
    }
    if (results_copy[i].artist.toLower() != request_.artist.toLower() && results_copy[i].album.toLower() != request_.album.toLower()) {
      results_copy[i].score -= 1.5;
    }
  }

  // Add results from the current provider to our pool
  results_.append(results_copy);
  statistics_.total_images_by_provider_[provider->name()]++;

  // Do we have more providers left?
  if (!pending_requests_.isEmpty()) {
    return;
  }

  AllProvidersFinished();

}

void AlbumCoverFetcherSearch::AllProvidersFinished() {

  if (cancel_requested_) {
    return;
  }

  // If we only wanted to do the search then we're done
  if (request_.search) {
    emit SearchFinished(request_.id, results_);
    return;
  }

  // No results?
  if (results_.isEmpty()) {
    statistics_.missing_images_++;
    emit AlbumCoverFetched(request_.id, QUrl(), QImage());
    return;
  }

  // Now we have to load some images and figure out which one is the best.
  // We'll sort the list of results by current score, then load the first 3 images from each category and use some heuristics for additional score.
  // If no images are good enough we'll keep loading more images until we find one that is or we run out of results.

  std::stable_sort(results_.begin(), results_.end(), CoverSearchResultCompareScore);

  FetchMoreImages();

}

void AlbumCoverFetcherSearch::FetchMoreImages() {

  int i = 0;
  while (!results_.isEmpty()) {
    ++i;
    CoverSearchResult result = results_.takeFirst();

    qLog(Debug) << "Loading" << result.image_url << "from" << result.provider << "with current score" << result.score;

    QNetworkRequest req(result.image_url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *image_reply = network_->get(req);
    connect(image_reply, &QNetworkReply::finished, [=] { ProviderCoverFetchFinished(image_reply); });
    pending_image_loads_[image_reply] = result;
    image_load_timeout_->AddReply(image_reply);

    ++statistics_.network_requests_made_;

    if (i >= 3) break;

  }

  if (pending_image_loads_.isEmpty()) {
    // There were no more results?  Time to give up.
    SendBestImage();
  }

}

void AlbumCoverFetcherSearch::ProviderCoverFetchFinished(QNetworkReply *reply) {

  disconnect(reply, &QNetworkReply::finished, this, nullptr);
  reply->deleteLater();

  if (!pending_image_loads_.contains(reply)) return;
  CoverSearchResult result = pending_image_loads_.take(reply);

  statistics_.bytes_transferred_ += reply->bytesAvailable();

  if (cancel_requested_) {
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << "Error requesting" << reply->url() << reply->errorString();
  }
  else {
    QImage image;
    if (image.loadFromData(reply->readAll())) {

      result.score += ScoreImage(image);
      candidate_images_.insertMulti(result.score, CandidateImage(result, image));

      qLog(Debug) << reply->url() << "from" << result.provider << "scored" << result.score;

    }
    else {
      qLog(Error) << "Error decoding image data from" << reply->url();
    }
  }

  if (pending_image_loads_.isEmpty()) {
    // We've fetched everything we wanted to fetch for now, check if we have an image that's good enough.
    float best_score = 0.0;

    if (!candidate_images_.isEmpty()) {
      best_score = candidate_images_.keys().last();
      qLog(Debug) << "Best image so far has a score of" << best_score;
    }

    if (best_score >= kGoodScore) {
      SendBestImage();
    }
    else {
      FetchMoreImages();
    }
  }

}

float AlbumCoverFetcherSearch::ScoreImage(const QImage &image) const {

  // Invalid images score nothing
  if (image.isNull()) {
    return 0.0;
  }

  // A 500x500px image scores 1.0, bigger scores higher
  const float size_score = std::sqrt(float(image.width() * image.height())) / kTargetSize;

  // A 1:1 image scores 1.0, anything else scores less
  const float aspect_score = 1.0 - float(std::max(image.width(), image.height()) - std::min(image.width(), image.height())) / std::max(image.height(), image.width());

  return size_score + aspect_score;

}

void AlbumCoverFetcherSearch::SendBestImage() {

  QUrl cover_url;
  QImage image;

  if (!candidate_images_.isEmpty()) {
    const CandidateImage best_image = candidate_images_.values().back();
    cover_url = best_image.first.image_url;
    image = best_image.second;

    qLog(Info) << "Using" << best_image.first.image_url << "from" << best_image.first.provider << "with score" << best_image.first.score;

    statistics_.chosen_images_by_provider_[best_image.first.provider]++;
    statistics_.chosen_images_++;
    statistics_.chosen_width_ += image.width();
    statistics_.chosen_height_ += image.height();
  }
  else {
    statistics_.missing_images_++;
  }

  emit AlbumCoverFetched(request_.id, cover_url, image);

}

void AlbumCoverFetcherSearch::Cancel() {

  cancel_requested_ = true;

  if (!pending_requests_.isEmpty()) {
    TerminateSearch();
  }
  else if (!pending_image_loads_.isEmpty()) {
    for (QNetworkReply *reply : pending_image_loads_.keys()) {
      disconnect(reply, &QNetworkReply::finished, this, nullptr);
      reply->abort();
      reply->deleteLater();
    }
    pending_image_loads_.clear();
  }

}

bool AlbumCoverFetcherSearch::ProviderCompareOrder(CoverProvider *a, CoverProvider *b) {
  return a->order() < b->order();
}

bool AlbumCoverFetcherSearch::CoverSearchResultCompareScore(const CoverSearchResult &a, const CoverSearchResult &b) {
  return a.score > b.score;
}
