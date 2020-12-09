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
#include <QMap>
#include <QMultiMap>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QImageReader>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>

#include "core/logging.h"
#include "core/utilities.h"
#include "core/network.h"
#include "core/networktimeouts.h"
#include "albumcoverfetcher.h"
#include "albumcoverfetchersearch.h"
#include "coverprovider.h"
#include "coverproviders.h"

const int AlbumCoverFetcherSearch::kSearchTimeoutMs = 20000;
const int AlbumCoverFetcherSearch::kImageLoadTimeoutMs = 6000;
const int AlbumCoverFetcherSearch::kTargetSize = 500;
const float AlbumCoverFetcherSearch::kGoodScore = 4.0;

AlbumCoverFetcherSearch::AlbumCoverFetcherSearch(const CoverSearchRequest &request, NetworkAccessManager *network, QObject *parent)
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

  // Ignore Radio Paradise "commercial" break.
  if (request_.artist.toLower() == "commercial-free" && request_.title.toLower() == "listener-supported") {
    TerminateSearch();
    return;
  }

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

    // If artist and album is missing, check if we can still use this provider by searching using title.
    if (!provider->allow_missing_album() && request_.album.isEmpty() && !request_.title.isEmpty()) {
      continue;
    }

    connect(provider, SIGNAL(SearchResults(int, CoverSearchResults)), SLOT(ProviderSearchResults(int, CoverSearchResults)));
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

void AlbumCoverFetcherSearch::ProviderSearchResults(const int id, const CoverSearchResults &results) {

  if (!pending_requests_.contains(id)) return;
  CoverProvider *provider = pending_requests_[id];
  ProviderSearchResults(provider, results);

}

void AlbumCoverFetcherSearch::ProviderSearchResults(CoverProvider *provider, const CoverSearchResults &results) {

  CoverSearchResults results_copy(results);
  for (int i = 0 ; i < results_copy.count() ; ++i) {

    results_copy[i].provider = provider->name();
    results_copy[i].score_provider = provider->quality();

    QString request_artist = request_.artist.toLower();
    QString request_album = request_.album.toLower();
    QString result_artist = results_copy[i].artist.toLower();
    QString result_album = results_copy[i].album.toLower();

    if (result_artist == request_artist) {
      results_copy[i].score_match += 0.5;
    }
    if (result_album == request_album) {
      results_copy[i].score_match += 0.5;
    }
    if (result_artist != request_artist && result_album != request_album) {
      results_copy[i].score_match -= 1.5;
    }

    if (request_album.isEmpty() && result_artist != request_artist) {
      results_copy[i].score_match -= 1;
    }

    // Decrease score if the search was based on artist and song title, and the resulting album is a compilation or live album.
    // This is done since we can't match the album titles, and we want to prevent compilation or live albums from being picked before studio albums for streams.
    // TODO: Make these regular expressions.
    if (request_album.isEmpty() && (
        result_album.contains("hits") ||
        result_album.contains("greatest") ||
        result_album.contains("best") ||
        result_album.contains("collection") ||
        result_album.contains("classics") ||
        result_album.contains("singles") ||
        result_album.contains("bootleg") ||
        result_album.contains("live") ||
        result_album.contains("concert") ||
        result_album.contains("essential") ||
        result_album.contains("ultimate") ||
        result_album.contains("karaoke") ||
        result_album.contains("mixtape") ||
        result_album.contains("country rock") ||
        result_album.contains("indie folk") ||
        result_album.contains("soft rock") ||
        result_album.contains("folk music") ||
        result_album.contains("60's rock") ||
        result_album.contains("60's romance") ||
        result_album.contains("60s music") ||
        result_album.contains("late 60s") ||
        result_album.contains("the 60s") ||
        result_album.contains("folk and blues") ||
        result_album.contains("60 from the 60's") ||
        result_album.contains("classic psychedelic") ||
        result_album.contains("playlist: acoustic") ||
        result_album.contains("90's rnb playlist") ||
        result_album.contains("rock 80s") ||
        result_album.contains("classic 80s") ||
        result_album.contains("rock anthems") ||
        result_album.contains("rock songs") ||
        result_album.contains("rock 2019") ||
        result_album.contains("guitar anthems") ||
        result_album.contains("driving anthems") ||
        result_album.contains("traffic jam jams") ||
        result_album.contains("perfect background music") ||
        result_album.contains("70's gold") ||
        result_album.contains("rockfluence") ||
        result_album.contains("acoustic dinner accompaniment") ||
        result_album.contains("complete studio albums") ||
        result_album.contains("mellow rock")
        )) {
      results_copy[i].score_match -= 1;
    }
    else if (request_album.isEmpty() && result_album.contains("soundtrack")) {
      results_copy[i].score_match -= 0.5;
    }

    // Set the initial image quality score besed on the size returned by the API, this is recalculated when the image is received.
    results_copy[i].score_quality += ScoreImage(results_copy[i].image_size);

  }

  // Add results from the current provider to our pool
  results_.append(results_copy);
  statistics_.total_images_by_provider_[provider->name()]++;

}

void AlbumCoverFetcherSearch::ProviderSearchFinished(const int id, const CoverSearchResults &results) {

  if (!pending_requests_.contains(id)) return;

  CoverProvider *provider = pending_requests_.take(id);
  ProviderSearchResults(provider, results);

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

    qLog(Debug) << "Loading" << result.artist << result.album << result.image_url << "from" << result.provider << "with current score" << result.score();

    QNetworkRequest req(result.image_url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
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

  disconnect(reply, nullptr, this, nullptr);
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
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    qLog(Error) << "Error requesting" << reply->url() << "received HTTP code" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }
  else {
    QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if (Utilities::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) || Utilities::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
      QImage image;
      if (image.loadFromData(reply->readAll())) {
        if (result.image_size != QSize(0,0) && result.image_size != image.size()) {
          qLog(Debug) << "API size for image" << result.image_size << "for" << reply->url() << "from" << result.provider << "did not match retrieved size" << image.size();
        }
        result.image_size = image.size();
        result.score_quality = ScoreImage(image.size());
        candidate_images_.insert(result.score(), CandidateImage(result, image));
        qLog(Debug) << reply->url() << "from" << result.provider << "scored" << result.score();
      }
      else {
        qLog(Error) << "Error decoding image data from" << reply->url();
      }
    }
    else {
      qLog(Error) << "Unsupported mimetype for image reader:" << mimetype << "from" << reply->url();
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

float AlbumCoverFetcherSearch::ScoreImage(const QSize size) const {

  if (size.width() == 0 || size.height() == 0) return 0.0;

  // A 500x500px image scores 1.0, bigger scores higher
  const float size_score = std::sqrt(float(size.width() * size.height())) / kTargetSize;

  // A 1:1 image scores 1.0, anything else scores less
  const float aspect_score = 1.0 - float(std::max(size.width(), size.height()) - std::min(size.width(), size.height())) / std::max(size.height(), size.width());

  return size_score + aspect_score;

}

void AlbumCoverFetcherSearch::SendBestImage() {

  QUrl cover_url;
  QImage image;

  if (!candidate_images_.isEmpty()) {
    const CandidateImage best_image = candidate_images_.values().back();
    cover_url = best_image.first.image_url;
    image = best_image.second;

    qLog(Info) << "Using" << best_image.first.image_url << "from" << best_image.first.provider << "with score" << best_image.first.score();

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
  return a.score() > b.score();
}

bool AlbumCoverFetcherSearch::CoverSearchResultCompareNumber(const CoverSearchResult &a, const CoverSearchResult &b) {
  return a.number < b.number;
}
