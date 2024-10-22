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

#include "includes/shared_ptr.h"
#include "lyricssearchrequest.h"
#include "lyricssearchresult.h"

class QTimer;
class LyricsProviders;
class LyricsFetcherSearch;

class LyricsFetcher : public QObject {
  Q_OBJECT

 public:
  explicit LyricsFetcher(const SharedPtr<LyricsProviders> lyrics_providers, QObject *parent = nullptr);
  ~LyricsFetcher() override {}

  struct Request {
    Request() : id(0) {}
    quint64 id;
    LyricsSearchRequest search_request;
  };

  quint64 Search(const QString &effective_albumartist, const QString &artist, const QString &album, const QString &title);
  void Clear();

 private:
  void AddRequest(const Request &request);

 Q_SIGNALS:
  void LyricsFetched(const quint64 request_id, const QString &provider, const QString &lyrics);
  void SearchFinished(const quint64 request_id, const LyricsSearchResults &results);

 private Q_SLOTS:
  void SingleSearchFinished(const quint64 request_id, const LyricsSearchResults &results);
  void SingleLyricsFetched(const quint64 request_id, const QString &provider, const QString &lyrics);
  void StartRequests();

 private:
  const SharedPtr<LyricsProviders> lyrics_providers_;
  quint64 next_id_;

  QQueue<Request> queued_requests_;
  QHash<quint64, LyricsFetcherSearch*> active_requests_;

  QTimer *request_starter_;
};

#endif  // LYRICSFETCHER_H
