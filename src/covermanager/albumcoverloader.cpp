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

#include <memory>
#include <chrono>

#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QSet>
#include <QQueue>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "utilities/mimeutils.h"
#include "utilities/imageutils.h"
#include "tagreader/tagreaderclient.h"
#include "albumcoverloader.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverloaderresult.h"
#include "albumcoverimageresult.h"

using namespace std::literals::chrono_literals;
using std::make_shared;

namespace {
constexpr int kMaxRedirects = 3;
}

AlbumCoverLoader::AlbumCoverLoader(const SharedPtr<TagReaderClient> tagreader_client, QObject *parent)
    : QObject(parent),
      tagreader_client_(tagreader_client),
      network_(new NetworkAccessManager(this)),
      timer_process_tasks_(new QTimer(this)),
      stop_requested_(false),
      load_image_async_id_(1),
      original_thread_(nullptr) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  original_thread_ = thread();

  timer_process_tasks_->setSingleShot(false);
  timer_process_tasks_->setInterval(10ms);
  QObject::connect(timer_process_tasks_, &QTimer::timeout, this, &AlbumCoverLoader::ProcessTasks);

}

void AlbumCoverLoader::ExitAsync() {

  stop_requested_ = true;
  QMetaObject::invokeMethod(this, &AlbumCoverLoader::Exit, Qt::QueuedConnection);

}

void AlbumCoverLoader::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());
  moveToThread(original_thread_);
  Q_EMIT ExitFinished();

}

void AlbumCoverLoader::CancelTask(const quint64 id) {

  QMutexLocker l(&mutex_load_image_async_);
  for (QQueue<TaskPtr>::iterator it = tasks_.begin(); it != tasks_.end(); ++it) {
    TaskPtr task = *it;
    if (task->id == id) {
      tasks_.erase(it);
      break;
    }
  }

}

void AlbumCoverLoader::CancelTasks(const QSet<quint64> &ids) {

  QMutexLocker l(&mutex_load_image_async_);
  for (QQueue<TaskPtr>::iterator it = tasks_.begin(); it != tasks_.end();) {
    TaskPtr task = *it;
    if (ids.contains(task->id)) {
      it = tasks_.erase(it);
    }
    else {
      ++it;
    }
  }

}

quint64 AlbumCoverLoader::LoadImageAsync(const AlbumCoverLoaderOptions &options, const Song &song) {

  TaskPtr task = make_shared<Task>();
  task->options = options;
  task->art_embedded = song.art_embedded();
  task->art_automatic = song.art_automatic();
  task->art_manual = song.art_manual();
  task->art_unset = song.art_unset();
  task->song_source = song.source();
  task->song_url = song.url();
  task->song = song;

  return EnqueueTask(task);

}

quint64 AlbumCoverLoader::LoadImageAsync(const AlbumCoverLoaderOptions &options, const bool art_embedded, const QUrl &art_automatic, const QUrl &art_manual, const bool art_unset, const QUrl &song_url, const Song::Source song_source) {

  TaskPtr task = make_shared<Task>();
  task->options = options;
  task->art_embedded = art_embedded;
  task->art_automatic = art_automatic;
  task->art_manual = art_manual;
  task->art_unset = art_unset;
  task->song_source = song_source;
  task->song_url = song_url;

  return EnqueueTask(task);

}

quint64 AlbumCoverLoader::LoadImageAsync(const AlbumCoverLoaderOptions &options, const AlbumCoverImageResult &album_cover) {

  TaskPtr task = make_shared<Task>();
  task->options = options;
  task->album_cover = album_cover;

  return EnqueueTask(task);

}

quint64 AlbumCoverLoader::LoadImageAsync(const AlbumCoverLoaderOptions &options, const QImage &image) {

  TaskPtr task = make_shared<Task>();
  task->options = options;
  task->album_cover.image = image;

  return EnqueueTask(task);

}

quint64 AlbumCoverLoader::EnqueueTask(TaskPtr task) {

  {
    QMutexLocker l(&mutex_load_image_async_);
    task->id = load_image_async_id_++;
    tasks_.enqueue(task);
  }

  QMetaObject::invokeMethod(this, &AlbumCoverLoader::StartProcessTasks, Qt::QueuedConnection);

  return task->id;

}

void AlbumCoverLoader::StartProcessTasks() {

  if (!timer_process_tasks_->isActive()) {
    timer_process_tasks_->start();
  }

}

void AlbumCoverLoader::ProcessTasks() {

  TaskPtr task;
  {
    QMutexLocker l(&mutex_load_image_async_);
    if (tasks_.isEmpty()) {
      if (timer_process_tasks_->isActive()) {
        timer_process_tasks_->stop();
      }
      return;
    }
    task = tasks_.dequeue();
  }

  ProcessTask(task);

}

void AlbumCoverLoader::ProcessTask(TaskPtr task) {

  // If we have album cover already, only do scale and pad.
  if (task->album_cover.is_valid()) {
    task->success = true;
  }
  else {
    InitArt(task);
  }

  while (!task->success && !task->options.types.isEmpty()) {
    const AlbumCoverLoaderOptions::Type type = task->options.types.takeFirst();
    const LoadImageResult result = LoadImage(task, type);
    if (result.status == LoadImageResult::Status::Async) {
      // The image is being loaded from a remote URL, we'll carry on later when it's done.
      return;
    }
    if (result.status == LoadImageResult::Status::Success) {
      task->success = true;
      task->result_type = result.type;
      break;
    }
  }

  if (!task->success && !task->options.default_cover.isEmpty()) {
    LoadLocalFileImage(task, AlbumCoverLoaderResult::Type::None, task->options.default_cover);
  }

  FinishTask(task, task->result_type);

}

void AlbumCoverLoader::FinishTask(TaskPtr task, const AlbumCoverLoaderResult::Type result_type) {

  QImage image_scaled;
  if (!task->album_cover.image_data.isEmpty() && !task->album_cover.image.isNull()) {
    task->result_type = result_type;
    task->album_cover.mime_type = Utilities::MimeTypeFromData(task->album_cover.image_data);
    if (task->scaled_image()) {
      image_scaled = ImageUtils::ScaleImage(task->album_cover.image, task->options.desired_scaled_size, task->options.device_pixel_ratio, task->pad_scaled_image());
    }
    if (!task->raw_image_data() && !task->album_cover.image_data.isNull()) {
      task->album_cover.image_data = QByteArray();
    }
    if (!task->original_image() && !task->album_cover.image.isNull()) {
      task->album_cover.image = QImage();
    }
  }

  Q_EMIT AlbumCoverLoaded(task->id, AlbumCoverLoaderResult(task->success, task->result_type, task->album_cover, image_scaled, task->art_manual_updated, task->art_automatic_updated));

}

void AlbumCoverLoader::InitArt(TaskPtr task) {

  // For local files and streams initialize art if found.
  if (task->song.is_valid() && (task->song.source() == Song::Source::LocalFile || task->song.is_radio()) && !task->song.art_manual_is_valid() && !task->song.art_automatic_is_valid()) {
    task->song.InitArtManual();
    if (task->song.art_manual_is_valid()) {
      task->art_manual_updated = task->song.art_manual();
      task->art_manual = task->song.art_manual();
    }
    if (task->song.url().isLocalFile()) {
      task->song.InitArtAutomatic();
      if (task->song.art_automatic_is_valid()) {
        task->art_automatic_updated = task->song.art_automatic();
        task->art_automatic = task->song.art_automatic();
      }
    }
  }

}

AlbumCoverLoader::LoadImageResult AlbumCoverLoader::LoadImage(TaskPtr task, const AlbumCoverLoaderOptions::Type type) {

  switch (type) {
    case AlbumCoverLoaderOptions::Type::Unset:{
      if (task->art_unset) {
        if (!task->options.default_cover.isEmpty()) {
          return LoadLocalFileImage(task, AlbumCoverLoaderResult::Type::Unset, task->options.default_cover);
        }
        return LoadImageResult(AlbumCoverLoaderResult::Type::Unset, LoadImageResult::Status::Success);
      }
      break;
    }
    case AlbumCoverLoaderOptions::Type::Embedded:{
      if (task->art_embedded && task->song_url.isValid() && task->song_url.isLocalFile()) {
        return LoadEmbeddedImage(task);
      }
      break;
    }
    case AlbumCoverLoaderOptions::Type::Automatic:{
      if (task->art_automatic.isValid()) {
        return LoadUrlImage(task, AlbumCoverLoaderResult::Type::Automatic, task->art_automatic);
      }
      break;
    }
    case AlbumCoverLoaderOptions::Type::Manual:{
      if (task->art_manual.isValid()) {
        return LoadUrlImage(task, AlbumCoverLoaderResult::Type::Manual, task->art_manual);
      }
      break;
    }
  }

  return LoadImageResult();

}

AlbumCoverLoader::LoadImageResult AlbumCoverLoader::LoadEmbeddedImage(TaskPtr task) {

  if (task->art_embedded && task->song_url.isValid() && task->song_url.isLocalFile()) {
    const TagReaderResult result = tagreader_client_->LoadCoverDataBlocking(task->song_url.toLocalFile(), task->album_cover.image_data);
    if (result.success() && !task->album_cover.image_data.isEmpty() && task->album_cover.image.loadFromData(task->album_cover.image_data)) {
      return LoadImageResult(AlbumCoverLoaderResult::Type::Embedded, LoadImageResult::Status::Success);
    }
  }

  return LoadImageResult(AlbumCoverLoaderResult::Type::Embedded, LoadImageResult::Status::Failure);

}

AlbumCoverLoader::LoadImageResult AlbumCoverLoader::LoadUrlImage(TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QUrl &cover_url) {

  if (cover_url.isValid()) {
    if (cover_url.isLocalFile()) {
      return LoadLocalUrlImage(task, result_type, cover_url);
    }
    if (network_->supportedSchemes().contains(cover_url.scheme())) {
      return LoadRemoteUrlImage(task, result_type, cover_url);
    }
  }

  return LoadImageResult(result_type, LoadImageResult::Status::Failure);

}

AlbumCoverLoader::LoadImageResult AlbumCoverLoader::LoadLocalUrlImage(TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QUrl &cover_url) {

  if (cover_url.isEmpty()) {
    return LoadImageResult(result_type, LoadImageResult::Status::Failure);
  }

  if (!cover_url.isValid()) {
    return LoadImageResult(result_type, LoadImageResult::Status::Failure);
  }

  if (!cover_url.isLocalFile()) {
    return LoadImageResult(result_type, LoadImageResult::Status::Failure);
  }

  return LoadLocalFileImage(task, result_type, cover_url.toLocalFile());

}

AlbumCoverLoader::LoadImageResult AlbumCoverLoader::LoadLocalFileImage(TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QString &cover_file) {

  if (!QFileInfo::exists(cover_file)) {
    qLog(Error) << "Cover file" << cover_file << "does not exist.";
    return LoadImageResult(result_type, LoadImageResult::Status::Failure);
  }

  QFile file(cover_file);
  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Unable to open cover file" << cover_file << "for reading:" << file.errorString();
    return LoadImageResult(result_type, LoadImageResult::Status::Failure);
  }

  task->album_cover.image_data = file.readAll();
  file.close();

  if (task->album_cover.image_data.isEmpty()) {
    qLog(Error) << "Cover file" << cover_file << "is empty.";
    return LoadImageResult(result_type, LoadImageResult::Status::Failure);
  }

  if (!task->album_cover.image.loadFromData(task->album_cover.image_data)) {
    qLog(Error) << "Failed to load image from cover file" << cover_file << ":" << file.errorString();
    return LoadImageResult(result_type, LoadImageResult::Status::Failure);
  }

  return LoadImageResult(result_type, LoadImageResult::Status::Success);

}

AlbumCoverLoader::LoadImageResult AlbumCoverLoader::LoadRemoteUrlImage(TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QUrl &cover_url) {

  qLog(Debug) << "Loading remote cover from URL" << cover_url;

  QNetworkRequest network_request(cover_url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, task, result_type, cover_url]() { LoadRemoteImageFinished(reply, task, result_type, cover_url); });

  return LoadImageResult(result_type, LoadImageResult::Status::Async);

}

void AlbumCoverLoader::LoadRemoteImageFinished(QNetworkReply *reply, TaskPtr task, const AlbumCoverLoaderResult::Type result_type, const QUrl &cover_url) {

  reply->deleteLater();

  QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
  if (redirect.isValid() && redirect.metaType().id() == QMetaType::QUrl) {
    if (task->redirects++ >= kMaxRedirects) {
      ProcessTask(task);
      return;
    }
    const QUrl redirect_url = redirect.toUrl();
    qLog(Debug) << "Loading remote cover from redirected URL" << redirect_url;
    QNetworkRequest network_request = reply->request();
    network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    network_request.setUrl(redirect_url);
    QNetworkReply *redirected_reply = network_->get(network_request);
    QObject::connect(redirected_reply, &QNetworkReply::finished, this, [this, reply, task, result_type, redirect_url]() { LoadRemoteImageFinished(reply, task, result_type, redirect_url); });
    return;
  }

  if (reply->error() == QNetworkReply::NoError) {
    task->album_cover.image_data = reply->readAll();
    if (!task->album_cover.image_data.isEmpty() && task->album_cover.image.loadFromData(task->album_cover.image_data)) {
      task->success = true;
      FinishTask(task, result_type);
      return;
    }
    else {
      qLog(Error) << "Unable to load album cover image from URL" << cover_url;
    }
  }
  else {
    qLog(Error) << "Unable to get album cover from URL" << cover_url << reply->error() << reply->errorString();
  }

  ProcessTask(task);

}
