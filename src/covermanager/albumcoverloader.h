/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ALBUMCOVERLOADER_H
#define ALBUMCOVERLOADER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QPair>
#include <QSet>
#include <QHash>
#include <QMultiMap>
#include <QQueue>
#include <QByteArray>
#include <QString>
#include <QImage>
#include <QPixmap>

#include "core/song.h"
#include "core/tagreaderclient.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverloaderresult.h"
#include "albumcoverimageresult.h"

class QThread;
class QNetworkReply;
class NetworkAccessManager;

class AlbumCoverLoader : public QObject {
  Q_OBJECT

 public:
  explicit AlbumCoverLoader(QObject *parent = nullptr);

  enum class State {
    None,
    Manual,
    Automatic
  };

  void ExitAsync();
  void Stop() { stop_requested_ = true; }

  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const Song &song);
  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const QUrl &art_automatic, const QUrl &art_manual, const QUrl &song_url = QUrl(), const Song::Source song_source = Song::Source::Unknown);
  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const AlbumCoverImageResult &album_cover);
  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const QImage &image);

  void CancelTask(const quint64 id);
  void CancelTasks(const QSet<quint64> &ids);

  quint64 SaveEmbeddedCoverAsync(const QString &song_filename, const QString &cover_filename);
  quint64 SaveEmbeddedCoverAsync(const QString &song_filename, const QImage &image);
  quint64 SaveEmbeddedCoverAsync(const QString &song_filename, const QByteArray &image_data);
  quint64 SaveEmbeddedCoverAsync(const QList<QUrl> &urls, const QString &cover_filename);
  quint64 SaveEmbeddedCoverAsync(const QList<QUrl> &urls, const QImage &image);
  quint64 SaveEmbeddedCoverAsync(const QList<QUrl> &urls, const QByteArray &image_data);

 signals:
  void ExitFinished();
  void AlbumCoverLoaded(quint64 id, AlbumCoverLoaderResult result);
  void SaveEmbeddedCoverAsyncFinished(quint64 id, bool success, bool cleared);

 protected slots:
  void Exit();
  void ProcessTasks();
  void RemoteFetchFinished(QNetworkReply *reply, const QUrl &cover_url);

  void SaveEmbeddedCover(const quint64 id, const QString &song_filename, const QString &cover_filename);
  void SaveEmbeddedCover(const quint64 id, const QString &song_filename, const QImage &image);
  void SaveEmbeddedCover(const quint64 id, const QString &song_filename, const QByteArray &image_data);
  void SaveEmbeddedCover(const quint64 id, const QList<QUrl> &urls, const QImage &image);
  void SaveEmbeddedCover(const quint64 id, const QList<QUrl> &urls, const QString &cover_filename);
  void SaveEmbeddedCover(const quint64 id, const QList<QUrl> &urls, const QByteArray &image_data);

  void SaveEmbeddedArtFinished(const quint64 id, TagReaderReply *reply, const bool cleared);

 protected:

  struct Task {
    explicit Task() : id(0), state(State::None), type(AlbumCoverLoaderResult::Type_None), art_updated(false), redirects(0) {}

    AlbumCoverLoaderOptions options;

    quint64 id;
    Song song;
    AlbumCoverImageResult album_cover;
    State state;
    AlbumCoverLoaderResult::Type type;
    bool art_updated;
    int redirects;
  };

  struct TryLoadResult {
    explicit TryLoadResult(const bool _started_async = false,
                           const bool _loaded_success = false,
                           const AlbumCoverLoaderResult::Type _type = AlbumCoverLoaderResult::Type_None,
                           const AlbumCoverImageResult &_album_cover = AlbumCoverImageResult()) :
                           started_async(_started_async),
                           loaded_success(_loaded_success),
                           type(_type),
                           album_cover(_album_cover) {}

    bool started_async;
    bool loaded_success;

    AlbumCoverLoaderResult::Type type;
    AlbumCoverImageResult album_cover;
  };

  quint64 EnqueueTask(Task &task);
  void ProcessTask(Task *task);
  void NextState(Task *task);
  TryLoadResult TryLoadImage(Task *task);

  bool stop_requested_;

  QMutex mutex_load_image_async_;
  QMutex mutex_save_image_async_;
  QQueue<Task> tasks_;
  QHash<QNetworkReply*, Task> remote_tasks_;
  quint64 load_image_async_id_;
  quint64 save_image_async_id_;

  NetworkAccessManager *network_;

  static const int kMaxRedirects = 3;

  QThread *original_thread_;

  QMultiMap<quint64, TagReaderReply*> tagreader_save_embedded_art_requests_;

};

#endif  // ALBUMCOVERLOADER_H
