/*
 * Strawberry Music Player
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QSet>
#include <QHash>
#include <QQueue>
#include <QByteArray>
#include <QString>
#include <QImage>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverloaderresult.h"
#include "albumcoverimageresult.h"

class QThread;
class QTimer;
class QNetworkReply;
class NetworkAccessManager;
class TagReaderClient;

class AlbumCoverLoader : public QObject {
  Q_OBJECT

 public:
  explicit AlbumCoverLoader(const SharedPtr<TagReaderClient> tagreader_client, QObject *parent = nullptr);

  void ExitAsync();
  void Stop() { stop_requested_ = true; }

  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const Song &song);
  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const bool art_embedded, const QUrl &art_automatic, const QUrl &art_manual, const bool art_unset, const QUrl &song_url = QUrl(), const Song::Source song_source = Song::Source::Unknown);
  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const AlbumCoverImageResult &album_cover);
  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const QImage &image);

  void CancelTask(const quint64 id);
  void CancelTasks(const QSet<quint64> &ids);

 Q_SIGNALS:
  void ExitFinished();
  void AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result);

 private:
  class Task {
   public:
    explicit Task() : id(0), success(false), art_embedded(false), art_unset(false), song_source(Song::Source::Unknown), result_type(AlbumCoverLoaderResult::Type::None), redirects(0) {}

    quint64 id;
    bool success;

    AlbumCoverLoaderOptions options;

    bool raw_image_data() const { return options.options & AlbumCoverLoaderOptions::Option::RawImageData; }
    bool original_image() const { return options.options & AlbumCoverLoaderOptions::Option::OriginalImage; }
    bool scaled_image() const { return options.options & AlbumCoverLoaderOptions::Option::ScaledImage; }
    bool pad_scaled_image() const { return options.options & AlbumCoverLoaderOptions::Option::PadScaledImage; }

    bool art_embedded;
    QUrl art_automatic;
    QUrl art_manual;
    bool art_unset;
    QUrl song_url;
    Song::Source song_source;
    Song song;
    AlbumCoverImageResult album_cover;
    AlbumCoverLoaderResult::Type result_type;
    QUrl art_manual_updated;
    QUrl art_automatic_updated;
    int redirects;
  };
  using TaskPtr = SharedPtr<Task>;

  class LoadImageResult {
   public:
    enum class Status {
      Failure,
      Async,
      Success
    };
    explicit LoadImageResult(AlbumCoverLoaderResult::Type _type = AlbumCoverLoaderResult::Type::None, Status _status = Status::Failure) : type(_type), status(_status) {}
    AlbumCoverLoaderResult::Type type;
    Status status;
  };

 private:
  quint64 EnqueueTask(TaskPtr task);
  void ProcessTask(TaskPtr task);
  void InitArt(TaskPtr task);
  LoadImageResult LoadImage(TaskPtr task, const AlbumCoverLoaderOptions::Type type);
  LoadImageResult LoadEmbeddedImage(TaskPtr task);
  LoadImageResult LoadUrlImage(TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QUrl &cover_url);
  LoadImageResult LoadLocalUrlImage(TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QUrl &cover_url);
  LoadImageResult LoadLocalFileImage(TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QString &cover_file);
  LoadImageResult LoadRemoteUrlImage(TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QUrl &cover_url);
  void FinishTask(TaskPtr task, const AlbumCoverLoaderResult::Type result_type);

 private Q_SLOTS:
  void Exit();
  void StartProcessTasks();
  void ProcessTasks();
  void LoadRemoteImageFinished(QNetworkReply *reply, AlbumCoverLoader::TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QUrl &cover_url);

 private:
  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<NetworkAccessManager> network_;
  QTimer *timer_process_tasks_;
  bool stop_requested_;
  QMutex mutex_load_image_async_;
  QQueue<TaskPtr> tasks_;
  quint64 load_image_async_id_;
  QThread *original_thread_;
};

#endif  // ALBUMCOVERLOADER_H
