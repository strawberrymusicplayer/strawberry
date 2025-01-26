/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CLOUDSTORAGESTREAMINGSERVICE_H
#define CLOUDSTORAGESTREAMINGSERVICE_H

#include <QList>

#include "includes/shared_ptr.h"
#include "tagreader/tagreaderclient.h"
#include "streamingservice.h"
#include "covermanager/albumcovermanager.h"
#include "collection/collectionmodel.h"

class TaskManager;
class Database;
class TagReaderClient;
class AlbumCoverLoader;
class CollectionBackend;
class CollectionModel;
class NetworkAccessManager;

class CloudStorageStreamingService : public StreamingService {
  Q_OBJECT

 public:
  explicit CloudStorageStreamingService(const SharedPtr<TaskManager> task_manager,
                                        const SharedPtr<Database> database,
                                        const SharedPtr<TagReaderClient> tagreader_client,
                                        const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                        const Song::Source source,
                                        const QString &name,
                                        const QString &url_scheme,
                                        const QString &settings_group,
                                        QObject *parent = nullptr);

  bool is_indexing() const { return indexing_task_id_ != -1; }

  SharedPtr<CollectionBackend> collection_backend() const { return collection_backend_; }
  CollectionModel *collection_model() const { return collection_model_; }
  CollectionFilter *collection_filter_model() const { return collection_model_->filter(); }

  SharedPtr<CollectionBackend> songs_collection_backend() override { return collection_backend_; }
  CollectionModel *songs_collection_model() override { return collection_model_; }
  CollectionFilter *songs_collection_filter_model() override { return collection_model_->filter(); }

  virtual void MaybeAddFileToDatabase(const QUrl &url, const QString &filename, const size_t size, const quint64 mtime, const QString &token_type = QString(), const QString &access_token = QString());
  static bool IsSupportedFiletype(const QString &filename);

 Q_SIGNALS:
  void AllIndexingTasksFinished();

 protected:
  void AbortReadTagsReplies();

 protected Q_SLOTS:
  void ReadStreamFinished(TagReaderReadStreamReplyPtr reply, const QUrl url, const QString filename);

 protected:
  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<CollectionBackend> collection_backend_;
  CollectionModel *collection_model_;
  QList<TagReaderReplyPtr> pending_tagreader_replies_;

 private:
  Song::Source source_;
  int indexing_task_id_;
  int indexing_task_progress_;
  int indexing_task_max_;
};

#endif  // CLOUDSTORAGESTREAMINGSERVICE_H
