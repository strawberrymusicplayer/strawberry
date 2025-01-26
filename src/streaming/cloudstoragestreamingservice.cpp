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

#include <memory>

#include <QString>
#include <QUrl>
#include <QFileInfo>

#include "core/logging.h"
#include "core/database.h"
#include "core/taskmanager.h"
#include "core/song.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "playlist/playlist.h"
#include "cloudstoragestreamingservice.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

CloudStorageStreamingService::CloudStorageStreamingService(const SharedPtr<TaskManager> task_manager,
                                                           const SharedPtr<Database> database,
                                                           const SharedPtr<TagReaderClient> tagreader_client,
                                                           const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                                           const Song::Source source,
                                                           const QString &name,
                                                           const QString &url_scheme,
                                                           const QString &settings_group,
                                                           QObject *parent)
    : StreamingService(source, name, url_scheme, settings_group, parent),
      task_manager_(task_manager),
      tagreader_client_(tagreader_client),
      source_(source),
      indexing_task_id_(-1),
      indexing_task_progress_(0),
      indexing_task_max_(0) {

  collection_backend_ = make_shared<CollectionBackend>();
  collection_backend_->moveToThread(database->thread());
  collection_backend_->Init(database, task_manager, source, name + "_songs"_L1);
  collection_model_ = new CollectionModel(collection_backend_, albumcover_loader, this);

}

void CloudStorageStreamingService::MaybeAddFileToDatabase(const QUrl &url, const QString &filename, const size_t size, const quint64 mtime, const QString &token_type, const QString &access_token) {

  if (!IsSupportedFiletype(filename)) {
    return;
  }

  if (indexing_task_id_ == -1) {
    indexing_task_id_ = task_manager_->StartTask(tr("Indexing %1").arg(name()));
    indexing_task_progress_ = 0;
    indexing_task_max_ = 0;
  }
  indexing_task_max_++;
  task_manager_->SetTaskProgress(indexing_task_id_, indexing_task_progress_, indexing_task_max_);

  TagReaderReadStreamReplyPtr reply = tagreader_client_->ReadStreamAsync(url, filename, size, mtime, token_type, access_token);
  pending_tagreader_replies_.append(reply);

  SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
  *connection = QObject::connect(&*reply, &TagReaderReadStreamReply::Finished, this, [this, reply, url, filename, connection]() {
    ReadStreamFinished(reply, url, filename);
    QObject::disconnect(*connection);
  }, Qt::QueuedConnection);

}

void CloudStorageStreamingService::ReadStreamFinished(TagReaderReadStreamReplyPtr reply, const QUrl url, const QString filename) {

  ++indexing_task_progress_;
  if (indexing_task_progress_ >= indexing_task_max_) {
    task_manager_->SetTaskFinished(indexing_task_id_);
    indexing_task_id_ = -1;
    Q_EMIT AllIndexingTasksFinished();
  }
  else {
    task_manager_->SetTaskProgress(indexing_task_id_, indexing_task_progress_, indexing_task_max_);
  }

  if (!reply->result().success()) {
    qLog(Error) << "Failed to read tags from stream, URL" << url << reply->result().error_string();
    return;
  }

  Song song = reply->song();
  song.set_source(source_);
  song.set_directory_id(0);
  QUrl song_url;
  song_url.setScheme(url_scheme());
  song_url.setPath(filename);
  song.set_url(song_url);

  collection_backend_->AddOrUpdateSongs(SongList() << song);

}

bool CloudStorageStreamingService::IsSupportedFiletype(const QString &filename) {

  const QFileInfo fileinfo(filename);
  return Song::kAcceptedExtensions.contains(fileinfo.suffix(), Qt::CaseInsensitive) && !Song::kRejectedExtensions.contains(fileinfo.suffix(), Qt::CaseInsensitive);

}

void CloudStorageStreamingService::AbortReadTagsReplies() {

  qLog(Debug) << "Aborting the read tags replies";

  pending_tagreader_replies_.clear();

  task_manager_->SetTaskFinished(indexing_task_id_);
  indexing_task_id_ = -1;

  Q_EMIT AllIndexingTasksFinished();

}
