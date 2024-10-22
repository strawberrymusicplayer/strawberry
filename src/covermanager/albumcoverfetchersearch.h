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

#ifndef ALBUMCOVERFETCHERSEARCH_H
#define ALBUMCOVERFETCHERSEARCH_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QPair>
#include <QMap>
#include <QMultiMap>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>

#include "includes/shared_ptr.h"
#include "albumcoverfetcher.h"
#include "coversearchstatistics.h"
#include "albumcoverimageresult.h"

class QNetworkReply;
class CoverProvider;
class CoverProviders;
class NetworkAccessManager;
class NetworkTimeouts;

// This class encapsulates a single search for covers initiated by an AlbumCoverFetcher.
// The search engages all of the known cover providers.
// AlbumCoverFetcherSearch signals search results to an interested AlbumCoverFetcher when all of the providers have done their part.
class AlbumCoverFetcherSearch : public QObject {
  Q_OBJECT

 public:
  explicit AlbumCoverFetcherSearch(const CoverSearchRequest &request, SharedPtr<NetworkAccessManager> network, QObject *parent);
  ~AlbumCoverFetcherSearch() override;

  void Start(SharedPtr<CoverProviders> cover_providers);

  // Cancels all pending requests.  No Finished signals will be emitted, and it is the caller's responsibility to delete the AlbumCoverFetcherSearch.
  void Cancel();

  CoverSearchStatistics statistics() const { return statistics_; }

  static bool CoverProviderSearchResultCompareNumber(const CoverProviderSearchResult &a, const CoverProviderSearchResult &b);

 Q_SIGNALS:
  // It's the end of search (when there was no fetch-me-a-cover request).
  void SearchFinished(quint64, const CoverProviderSearchResults &results);

  // It's the end of search and we've fetched a cover.
  void AlbumCoverFetched(const quint64 id, const AlbumCoverImageResult &result);

 private Q_SLOTS:
  void ProviderSearchResults(const int id, const CoverProviderSearchResults &results);
  void ProviderSearchFinished(const int id, const CoverProviderSearchResults &results);
  void ProviderCoverFetchFinished(QNetworkReply *reply);
  void TerminateSearch();

 private:
  void ProviderSearchResults(CoverProvider *provider, const CoverProviderSearchResults &results);
  void AllProvidersFinished();

  void FetchMoreImages();
  static float ScoreImage(const QSize size);
  void SendBestImage();

  static bool ProviderCompareOrder(CoverProvider *a, CoverProvider *b);
  static bool CoverProviderSearchResultCompareScore(const CoverProviderSearchResult &a, const CoverProviderSearchResult &b);

 private:
  CoverSearchStatistics statistics_;

  // Search request encapsulated by this AlbumCoverFetcherSearch.
  CoverSearchRequest request_;

  // Complete results (from all of the available providers).
  CoverProviderSearchResults results_;

  QMap<int, CoverProvider*> pending_requests_;
  QHash<QNetworkReply*, CoverProviderSearchResult> pending_image_loads_;
  NetworkTimeouts *image_load_timeout_;

  // QMap is sorted by key (score).
  struct CandidateImage {
    CandidateImage(const CoverProviderSearchResult &_result, const AlbumCoverImageResult &_album_cover) : result(_result), album_cover(_album_cover) {}
    CoverProviderSearchResult result;
    AlbumCoverImageResult album_cover;
  };
  QMultiMap<float, CandidateImage> candidate_images_;

  SharedPtr<NetworkAccessManager> network_;

  bool cancel_requested_;

};

#endif  // ALBUMCOVERFETCHERSEARCH_H
