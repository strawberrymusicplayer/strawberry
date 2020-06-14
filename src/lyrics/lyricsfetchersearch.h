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

#ifndef LYRICSFETCHERSEARCH_H
#define LYRICSFETCHERSEARCH_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMap>
#include <QString>

#include "lyricsfetcher.h"

class LyricsProvider;
class LyricsProviders;

class LyricsFetcherSearch : public QObject {
  Q_OBJECT

 public:
  explicit LyricsFetcherSearch(const LyricsSearchRequest &request, QObject *parent);

  void Start(LyricsProviders *lyrics_providers);
  void Cancel();

 signals:
  void SearchFinished(const quint64, const LyricsSearchResults &results);
  void LyricsFetched(const quint64, const QString &provider, const QString &lyrics);

 private slots:
  void ProviderSearchFinished(const quint64 id, const LyricsSearchResults &results);
  void TerminateSearch();

 private:
  void AllProvidersFinished();
  static bool ProviderCompareOrder(LyricsProvider *a, LyricsProvider *b);
  static bool LyricsSearchResultCompareScore(const LyricsSearchResult &a, const LyricsSearchResult &b);

 private:
  static const int kSearchTimeoutMs;
  static const int kGoodLyricsLength;
  static const float kHighScore;

  LyricsSearchRequest request_;
  LyricsSearchResults results_;
  QMap<int, LyricsProvider*> pending_requests_;
  bool cancel_requested_;

};

#endif  // LYRICSFETCHERSEARCH_H
