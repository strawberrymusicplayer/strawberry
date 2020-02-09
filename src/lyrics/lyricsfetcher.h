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

#ifndef LYRICSFETCHER_H
#define LYRICSFETCHER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMetaType>
#include <QQueue>
#include <QSet>
#include <QList>
#include <QHash>
#include <QString>
#include <QUrl>

class QTimer;
class LyricsProviders;
class LyricsFetcherSearch;

struct LyricsSearchRequest {
  LyricsSearchRequest() : id(-1) {}
  quint64 id;
  QString artist;
  QString album;
  QString title;
};

struct LyricsSearchResult {
  LyricsSearchResult() : score(0.0) {}
  QString provider;
  QString artist;
  QString album;
  QString title;
  QString lyrics;
  float score;
};
Q_DECLARE_METATYPE(LyricsSearchResult)

typedef QList<LyricsSearchResult> LyricsSearchResults;
Q_DECLARE_METATYPE(QList<LyricsSearchResult>)

class LyricsFetcher : public QObject {
  Q_OBJECT

 public:
  LyricsFetcher(LyricsProviders *lyrics_providers, QObject *parent = nullptr);
  virtual ~LyricsFetcher() {}

  static const int kMaxConcurrentRequests;
  static const int kGoodLyricsLength;

  quint64 Search(const QString &artist, const QString &album, const QString &title);
  void Clear();

signals:
  void LyricsFetched(const quint64 request_id, const QString &provider, const QString &lyrics);
  void SearchFinished(const quint64 request_id, const LyricsSearchResults &results);

 private slots:
  void SingleSearchFinished(const quint64 request_id, const LyricsSearchResults &results);
  void SingleLyricsFetched(const quint64 request_id, const QString &provider, const QString &lyrics);
  void StartRequests();

 private:
  void AddRequest(const LyricsSearchRequest &req);

  LyricsProviders *lyrics_providers_;
  quint64 next_id_;

  QQueue<LyricsSearchRequest> queued_requests_;
  QHash<quint64, LyricsFetcherSearch*> active_requests_;

  QTimer *request_starter_;

};

#endif  // LYRICSFETCHER_H
