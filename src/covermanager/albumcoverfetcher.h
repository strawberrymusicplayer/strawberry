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

#ifndef ALBUMCOVERFETCHER_H
#define ALBUMCOVERFETCHER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMetaType>
#include <QSet>
#include <QList>
#include <QHash>
#include <QQueue>
#include <QString>
#include <QUrl>
#include <QImage>

class QTimer;
class NetworkAccessManager;
class CoverProviders;
class AlbumCoverFetcherSearch;
struct CoverSearchStatistics;

// This class represents a single search-for-cover request. It identifies and describes the request.
struct CoverSearchRequest {
  explicit CoverSearchRequest() : id(-1), search(false), fetchall(false) {}

  // An unique (for one AlbumCoverFetcher) request identifier
  quint64 id;

  // A search query
  QString artist;
  QString album;
  QString title;

  // Is this only a search request or should we also fetch the first cover that's found?
  bool search;

  // Is the request part of fetchall (fetching all missing covers)
  bool fetchall;
};

// This structure represents a single result of some album's cover search request.
struct CoverSearchResult {
  explicit CoverSearchResult() : score_provider(0.0), score_match(0.0), score_quality(0.0), number(0) {}

  // Used for grouping in the user interface.
  QString provider;

  // Artist and album returned by the provider
  QString artist;
  QString album;

  // An URL of a cover image
  QUrl image_url;

  // Image size
  QSize image_size;

  // Score for this provider
  float score_provider;

  // Score for match
  float score_match;

  // Score for image quality
  float score_quality;

  // The result number
  int number;

  // Total score for this result
  float score() const { return score_provider + score_match + score_quality; }

};
Q_DECLARE_METATYPE(CoverSearchResult)

// This is a complete result of a single search request (a list of results, each describing one image, actually).
typedef QList<CoverSearchResult> CoverSearchResults;
Q_DECLARE_METATYPE(QList<CoverSearchResult>)

// This class searches for album covers for a given query or artist/album and returns URLs. It's NOT thread-safe.
class AlbumCoverFetcher : public QObject {
  Q_OBJECT

 public:
  explicit AlbumCoverFetcher(CoverProviders *cover_providers, QObject *parent = nullptr, NetworkAccessManager *network = nullptr);
  ~AlbumCoverFetcher() override;

  static const int kMaxConcurrentRequests;

  quint64 SearchForCovers(const QString &artist, const QString &album, const QString &title = QString());
  quint64 FetchAlbumCover(const QString &artist, const QString &album, const QString &title, const bool fetchall);

  void Clear();

 signals:
  void AlbumCoverFetched(const quint64 request_id, const QUrl &cover_url, const QImage &cover, const CoverSearchStatistics &statistics);
  void SearchFinished(const quint64 request_id, const CoverSearchResults &results, const CoverSearchStatistics &statistics);

 private slots:
  void SingleSearchFinished(const quint64, const CoverSearchResults results);
  void SingleCoverFetched(const quint64, const QUrl &cover_url, const QImage &image);
  void StartRequests();

 private:
  void AddRequest(const CoverSearchRequest &req);

  CoverProviders *cover_providers_;
  NetworkAccessManager *network_;
  quint64 next_id_;

  QQueue<CoverSearchRequest> queued_requests_;
  QHash<quint64, AlbumCoverFetcherSearch*> active_requests_;

  QTimer *request_starter_;

};

#endif  // ALBUMCOVERFETCHER_H
