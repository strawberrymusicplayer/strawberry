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

#include <cmath>
#include <algorithm>
#include <utility>

#include <QObject>
#include <QCoreApplication>
#include <QTimer>
#include <QList>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QImageReader>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/networktimeouts.h"
#include "utilities/imageutils.h"
#include "utilities/mimeutils.h"
#include "albumcoverfetcher.h"
#include "albumcoverfetchersearch.h"
#include "coverprovider.h"
#include "coverproviders.h"
#include "albumcoverimageresult.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kSearchTimeoutMs = 20000;
constexpr int kImageLoadTimeoutMs = 6000;
constexpr int kTargetSize = 500;
constexpr float kGoodScore = 4.0;
}  // namespace

AlbumCoverFetcherSearch::AlbumCoverFetcherSearch(const CoverSearchRequest &request, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      request_(request),
      image_load_timeout_(new NetworkTimeouts(kImageLoadTimeoutMs, this)),
      network_(network),
      cancel_requested_(false) {

  // We will terminate the search after kSearchTimeoutMs milliseconds if we are not able to find all of the results before that point in time
  QTimer::singleShot(kSearchTimeoutMs, this, &AlbumCoverFetcherSearch::TerminateSearch);

}

AlbumCoverFetcherSearch::~AlbumCoverFetcherSearch() {
  pending_requests_.clear();
  Cancel();
}

void AlbumCoverFetcherSearch::TerminateSearch() {

  const QList<int> ids = pending_requests_.keys();
  for (const int id : ids) {
    pending_requests_.take(id)->CancelSearch(id);
  }

  AllProvidersFinished();

}

void AlbumCoverFetcherSearch::Start(SharedPtr<CoverProviders> cover_providers) {

  // Ignore Radio Paradise "commercial" break.
  if (request_.artist.compare("commercial-free"_L1, Qt::CaseInsensitive) == 0 && request_.title.compare("listener-supported"_L1, Qt::CaseInsensitive) == 0) {
    TerminateSearch();
    return;
  }

  QList<CoverProvider*> cover_providers_sorted = cover_providers->List();
  std::stable_sort(cover_providers_sorted.begin(), cover_providers_sorted.end(), ProviderCompareOrder);

  for (CoverProvider *provider : std::as_const(cover_providers_sorted)) {

    if (!provider->enabled()) continue;

    // Skip any provider that requires authentication but is not authenticated.
    if (provider->authentication_required() && !provider->authenticated()) {
      continue;
    }

    // Skip provider if it does not have batch set and we are doing a batch - "Fetch Missing Covers".
    if (!provider->batch() && request_.batch) {
      continue;
    }

    // If artist and album is missing, check if we can still use this provider by searching using title.
    if (!provider->allow_missing_album() && request_.album.isEmpty() && !request_.title.isEmpty()) {
      continue;
    }

    QObject::connect(provider, &CoverProvider::SearchResults, this, QOverload<const int, const CoverProviderSearchResults&>::of(&AlbumCoverFetcherSearch::ProviderSearchResults));
    QObject::connect(provider, &CoverProvider::SearchFinished, this, &AlbumCoverFetcherSearch::ProviderSearchFinished);
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

void AlbumCoverFetcherSearch::ProviderSearchResults(const int id, const CoverProviderSearchResults &results) {

  if (!pending_requests_.contains(id)) return;
  CoverProvider *provider = pending_requests_.value(id);
  ProviderSearchResults(provider, results);

}

void AlbumCoverFetcherSearch::ProviderSearchResults(CoverProvider *provider, const CoverProviderSearchResults &results) {

  CoverProviderSearchResults results_copy(results);
  for (int i = 0; i < results_copy.count(); ++i) {

    results_copy[i].provider = provider->name();
    results_copy[i].score_provider = provider->quality();

    const QString &request_artist = request_.artist;
    const QString &request_album = request_.album;
    const QString &result_artist = results_copy[i].artist;
    const QString &result_album = results_copy[i].album;

    if (result_artist.compare(request_artist, Qt::CaseInsensitive) == 0) {
      results_copy[i].score_match += 0.5;
    }
    if (result_album.compare(request_album, Qt::CaseInsensitive) == 0) {
      results_copy[i].score_match += 0.5;
    }
    if (result_artist.compare(request_artist, Qt::CaseInsensitive) != 0 && result_album.compare(request_album, Qt::CaseInsensitive) != 0) {
      results_copy[i].score_match -= 1.5;
    }

    if (request_album.isEmpty() && result_artist.compare(request_artist, Qt::CaseInsensitive) != 0) {
      results_copy[i].score_match -= 1;
    }

    // Decrease score if the search was based on artist and song title, and the resulting album is a compilation or live album.
    // This is done since we can't match the album titles, and we want to prevent compilation or live albums from being picked before studio albums for streams.
    // TODO: Make these regular expressions.
    if (request_album.isEmpty() && (
        result_album.contains("hits"_L1, Qt::CaseInsensitive) ||
        result_album.contains("greatest"_L1, Qt::CaseInsensitive) ||
        result_album.contains("best"_L1, Qt::CaseInsensitive) ||
        result_album.contains("collection"_L1, Qt::CaseInsensitive) ||
        result_album.contains("classics"_L1, Qt::CaseInsensitive) ||
        result_album.contains("singles"_L1, Qt::CaseInsensitive) ||
        result_album.contains("bootleg"_L1, Qt::CaseInsensitive) ||
        result_album.contains("live"_L1, Qt::CaseInsensitive) ||
        result_album.contains("concert"_L1, Qt::CaseInsensitive) ||
        result_album.contains("essential"_L1, Qt::CaseInsensitive) ||
        result_album.contains("ultimate"_L1, Qt::CaseInsensitive) ||
        result_album.contains("karaoke"_L1, Qt::CaseInsensitive) ||
        result_album.contains("mixtape"_L1, Qt::CaseInsensitive) ||
        result_album.contains("country rock"_L1, Qt::CaseInsensitive) ||
        result_album.contains("indie folk"_L1, Qt::CaseInsensitive) ||
        result_album.contains("soft rock"_L1, Qt::CaseInsensitive) ||
        result_album.contains("folk music"_L1, Qt::CaseInsensitive) ||
        result_album.contains("60's rock"_L1, Qt::CaseInsensitive) ||
        result_album.contains("60's romance"_L1, Qt::CaseInsensitive) ||
        result_album.contains("60s music"_L1, Qt::CaseInsensitive) ||
        result_album.contains("late 60s"_L1, Qt::CaseInsensitive) ||
        result_album.contains("the 60s"_L1, Qt::CaseInsensitive) ||
        result_album.contains("folk and blues"_L1, Qt::CaseInsensitive) ||
        result_album.contains("60 from the 60's"_L1, Qt::CaseInsensitive) ||
        result_album.contains("classic psychedelic"_L1, Qt::CaseInsensitive) ||
        result_album.contains("playlist: acoustic"_L1, Qt::CaseInsensitive) ||
        result_album.contains("90's rnb playlist"_L1, Qt::CaseInsensitive) ||
        result_album.contains("rock 80s"_L1, Qt::CaseInsensitive) ||
        result_album.contains("classic 80s"_L1, Qt::CaseInsensitive) ||
        result_album.contains("rock anthems"_L1, Qt::CaseInsensitive) ||
        result_album.contains("rock songs"_L1, Qt::CaseInsensitive) ||
        result_album.contains("rock 2019"_L1, Qt::CaseInsensitive) ||
        result_album.contains("guitar anthems"_L1, Qt::CaseInsensitive) ||
        result_album.contains("driving anthems"_L1, Qt::CaseInsensitive) ||
        result_album.contains("traffic jam jams"_L1, Qt::CaseInsensitive) ||
        result_album.contains("perfect background music"_L1, Qt::CaseInsensitive) ||
        result_album.contains("70's gold"_L1, Qt::CaseInsensitive) ||
        result_album.contains("rockfluence"_L1, Qt::CaseInsensitive) ||
        result_album.contains("acoustic dinner accompaniment"_L1, Qt::CaseInsensitive) ||
        result_album.contains("complete studio albums"_L1, Qt::CaseInsensitive) ||
        result_album.contains("mellow rock"_L1, Qt::CaseInsensitive)
        )) {
      results_copy[i].score_match -= 1;
    }
    else if (request_album.isEmpty() && result_album.contains("soundtrack"_L1, Qt::CaseInsensitive)) {
      results_copy[i].score_match -= 0.5;
    }

    // Set the initial image quality score based on the size returned by the API, this is recalculated when the image is received.
    results_copy[i].score_quality += ScoreImage(results_copy[i].image_size);

  }

  // Add results from the current provider to our pool
  results_.append(results_copy);
  statistics_.total_images_by_provider_[provider->name()]++;

}

void AlbumCoverFetcherSearch::ProviderSearchFinished(const int id, const CoverProviderSearchResults &results) {

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

  qLog(Debug) << "Search finished, got" << results_.count() << "results";

  if (cancel_requested_) {
    return;
  }

  // If we only wanted to do the search then we're done
  if (request_.search) {
    Q_EMIT SearchFinished(request_.id, results_);
    return;
  }

  // No results?
  if (results_.isEmpty()) {
    statistics_.missing_images_++;
    Q_EMIT AlbumCoverFetched(request_.id, AlbumCoverImageResult());
    return;
  }

  // Now we have to load some images and figure out which one is the best.
  // We'll sort the list of results by current score, then load the first 3 images from each category and use some heuristics for additional score.
  // If no images are good enough we'll keep loading more images until we find one that is or we run out of results.

  std::stable_sort(results_.begin(), results_.end(), CoverProviderSearchResultCompareScore);

  FetchMoreImages();

}

void AlbumCoverFetcherSearch::FetchMoreImages() {

  int i = 0;
  while (!results_.isEmpty()) {
    ++i;
    CoverProviderSearchResult result = results_.takeFirst();

    qLog(Debug) << "Loading" << result.artist << result.album << result.image_url << "from" << result.provider << "with current score" << result.score();

    QNetworkRequest network_request(result.image_url);
    network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *image_reply = network_->get(network_request);
    QObject::connect(image_reply, &QNetworkReply::finished, this, [this, image_reply]() { ProviderCoverFetchFinished(image_reply); });
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

  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (!pending_image_loads_.contains(reply)) return;
  CoverProviderSearchResult result = pending_image_loads_.take(reply);

  statistics_.bytes_transferred_ += reply->bytesAvailable();

  if (cancel_requested_) return;

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << "Error requesting" << reply->url() << reply->errorString();
  }
  else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    qLog(Error) << "Error requesting" << reply->url() << "received HTTP code" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }
  else {
    QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if (mimetype.contains(u';')) {
      mimetype = mimetype.left(mimetype.indexOf(u';'));
    }
    if (ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) || ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
      QByteArray image_data = reply->readAll();
      QString mime_type = Utilities::MimeTypeFromData(image_data);
      QImage image;
      if (image.loadFromData(image_data)) {
        if (result.image_size != QSize(0, 0) && result.image_size != image.size()) {
          qLog(Debug) << "API size for image" << result.image_size << "for" << reply->url() << "from" << result.provider << "did not match retrieved size" << image.size();
        }
        result.image_size = image.size();
        result.score_quality = ScoreImage(image.size());
        candidate_images_.insert(result.score(), CandidateImage(result, AlbumCoverImageResult(result.image_url, mime_type, image_data, image)));
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
      QList<float> scores = candidate_images_.keys();
      best_score = scores.last();
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

float AlbumCoverFetcherSearch::ScoreImage(const QSize size) {

  if (size.width() == 0 || size.height() == 0) return 0.0;

  // A 500x500px image scores 1.0, bigger scores higher
  const float size_score = std::sqrt(static_cast<float>(size.width() * size.height())) / kTargetSize;

  // A 1:1 image scores 1.0, anything else scores less
  const float aspect_score = static_cast<float>(1.0) - static_cast<float>(std::max(size.width(), size.height()) - std::min(size.width(), size.height())) / static_cast<float>(std::max(size.height(), size.width()));

  return size_score + aspect_score;

}

void AlbumCoverFetcherSearch::SendBestImage() {

  AlbumCoverImageResult result;

  if (!candidate_images_.isEmpty()) {
    QList<CandidateImage> candidate_images = candidate_images_.values();
    const CandidateImage best_image = candidate_images.back();
    result = best_image.album_cover;

    qLog(Info) << "Using" << best_image.result.image_url << "from" << best_image.result.provider << "with score" << best_image.result.score();

    statistics_.chosen_images_by_provider_[best_image.result.provider]++;
    statistics_.chosen_images_++;
    statistics_.chosen_width_ += result.image.width();
    statistics_.chosen_height_ += result.image.height();
  }
  else {
    statistics_.missing_images_++;
  }

  Q_EMIT AlbumCoverFetched(request_.id, result);

}

void AlbumCoverFetcherSearch::Cancel() {

  cancel_requested_ = true;

  if (!pending_requests_.isEmpty()) {
    TerminateSearch();
  }
  else if (!pending_image_loads_.isEmpty()) {
    const QList<QNetworkReply*> replies = pending_image_loads_.keys();
    for (QNetworkReply *reply : replies) {
      QObject::disconnect(reply, &QNetworkReply::finished, this, nullptr);
      reply->abort();
      reply->deleteLater();
    }
    pending_image_loads_.clear();
  }

}

bool AlbumCoverFetcherSearch::ProviderCompareOrder(CoverProvider *a, CoverProvider *b) {
  return a->order() < b->order();
}

bool AlbumCoverFetcherSearch::CoverProviderSearchResultCompareScore(const CoverProviderSearchResult &a, const CoverProviderSearchResult &b) {
  return a.score() > b.score();
}

bool AlbumCoverFetcherSearch::CoverProviderSearchResultCompareNumber(const CoverProviderSearchResult &a, const CoverProviderSearchResult &b) {
  return a.number < b.number;
}
