/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef INTERNETSEARCH_H
#define INTERNETSEARCH_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QImage>
#include <QPixmap>
#include <QPixmapCache>

#include "core/song.h"
#include "covermanager/albumcoverloaderoptions.h"

class Application;
class MimeData;
class AlbumCoverLoader;
class InternetService;

class InternetSearch : public QObject {
  Q_OBJECT

 public:
  InternetSearch(Application *app, Song::Source source, QObject *parent = nullptr);
  ~InternetSearch();

  enum SearchType {
    SearchType_Artists = 1,
    SearchType_Albums = 2,
    SearchType_Songs = 3,
  };

  struct Result {
    Song metadata_;
    QString pixmap_cache_key_;
  };
  typedef QList<Result> ResultList;

  static const int kDelayedSearchTimeoutMs;

  Application *application() const { return app_; }
  Song::Source source() const { return source_; }
  InternetService *service() const { return service_; }

  int SearchAsync(const QString &query, SearchType type);
  int LoadArtAsync(const InternetSearch::Result &result);

  void CancelSearch(const int id);
  void CancelArt(const int id);

  // Loads tracks for results that were previously emitted by ResultsAvailable.
  // The implementation creates a SongMimeData with one Song for each Result.
  MimeData *LoadTracks(const ResultList &results);

 signals:
  void SearchAsyncSig(const int id, const QString &query, const SearchType type);
  void ResultsAvailable(const int id, const InternetSearch::ResultList &results);
  void AddResults(const int id, const InternetSearch::ResultList &results);
  void SearchError(const int id, const QString &error);
  void SearchFinished(const int id);
  void UpdateStatus(const int id, const QString &text);
  void ProgressSetMaximum(const int id, const int progress);
  void UpdateProgress(const int id, const int max);

  void ArtLoaded(const int id, const QPixmap &pixmap);

 protected:

  struct PendingState {
    PendingState() : orig_id_(-1) {}
    PendingState(int orig_id, QStringList tokens)
        : orig_id_(orig_id), tokens_(tokens) {}
    int orig_id_;
    QStringList tokens_;

    bool operator<(const PendingState &b) const {
      return orig_id_ < b.orig_id_;
    }

    bool operator==(const PendingState &b) const {
      return orig_id_ == b.orig_id_;
    }
  };

  void timerEvent(QTimerEvent *e);

  // These functions treat queries in the same way as CollectionQuery.
  // They're useful for figuring out whether you got a result because it matched in the song title or the artist/album name.
  static QStringList TokenizeQuery(const QString &query);
  static bool Matches(const QStringList &tokens, const QString &string);

 private slots:
  void DoSearchAsync(const int id, const QString &query, const SearchType type);
  void SearchDone(const int service_id, const SongList &songs, const QString &error);

  void AlbumArtLoaded(const quint64 id, const QImage &image);

  void UpdateStatusSlot(const int id, const QString &text);
  void ProgressSetMaximumSlot(const int id, const int progress);
  void UpdateProgressSlot(const int id, const int max);

 private:
  void SearchAsync(const int id, const QString &query, const SearchType type);
  bool FindCachedPixmap(const InternetSearch::Result &result, QPixmap *pixmap) const;
  QString PixmapCacheKey(const InternetSearch::Result &result) const;
  void MaybeSearchFinished(const int id);
  void ShowConfig() {}
  static QImage ScaleAndPad(const QImage &image);

 private:
  struct DelayedSearch {
    int id_;
    QString query_;
    SearchType type_;
  };

  static const int kArtHeight;

  Application *app_;
  Song::Source source_;
  InternetService *service_;
  int searches_next_id_;
  int art_searches_next_id_;

  QMap<int, DelayedSearch> delayed_searches_;
  QMap<int, QString> pending_art_searches_;
  QPixmapCache pixmap_cache_;
  AlbumCoverLoaderOptions cover_loader_options_;
  QMap<quint64, int> cover_loader_tasks_;

  QMap<int, PendingState> pending_searches_;

};

Q_DECLARE_METATYPE(InternetSearch::Result)
Q_DECLARE_METATYPE(InternetSearch::ResultList)

#endif  // INTERNETSEARCH_H
