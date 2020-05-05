/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QStandardPaths>
#include <QDir>
#include <QThread>
#include <QMutex>
#include <QSet>
#include <QList>
#include <QQueue>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QSize>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

#include "core/network.h"
#include "core/song.h"
#include "core/tagreaderclient.h"
#include "core/utilities.h"
#include "settings/collectionsettingspage.h"
#include "organise/organiseformat.h"
#include "albumcoverloader.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverloaderresult.h"

AlbumCoverLoader::AlbumCoverLoader(QObject *parent)
    : QObject(parent),
      stop_requested_(false),
      next_id_(1),
      network_(new NetworkAccessManager(this)),
      cover_album_dir_(false),
      cover_filename_(CollectionSettingsPage::SaveCover_Hash),
      cover_overwrite_(false),
      cover_lowercase_(true),
      cover_replace_spaces_(true),
      original_thread_(nullptr)
      {

  original_thread_ = thread();
  ReloadSettings();

}

void AlbumCoverLoader::ExitAsync() {

  stop_requested_ = true;
  metaObject()->invokeMethod(this, "Exit", Qt::QueuedConnection);

}

void AlbumCoverLoader::Exit() {

  assert(QThread::currentThread() == thread());
  moveToThread(original_thread_);
  emit ExitFinished();

}

void AlbumCoverLoader::ReloadSettings() {

  QSettings s;
  s.beginGroup(CollectionSettingsPage::kSettingsGroup);
  cover_album_dir_ = s.value("cover_album_dir", false).toBool();
  cover_filename_ = CollectionSettingsPage::SaveCover(s.value("cover_filename", CollectionSettingsPage::SaveCover_Hash).toInt());
  cover_pattern_ = s.value("cover_pattern", "%albumartist-%album").toString();
  cover_overwrite_ = s.value("cover_overwrite", false).toBool();
  cover_lowercase_ = s.value("cover_lowercase", false).toBool();
  cover_replace_spaces_ = s.value("cover_replace_spaces", false).toBool();
  s.endGroup();

}

QString AlbumCoverLoader::AlbumCoverFilename(QString artist, QString album) {

  artist.remove('/');
  album.remove('/');

  QString filename = artist + "-" + album + ".jpg";
  filename = Utilities::UnicodeToAscii(filename.toLower());
  filename = filename.replace(' ', '-');
  filename = filename.replace("--", "-");
  filename = filename.remove(OrganiseFormat::kInvalidFatCharacters);
  filename = filename.trimmed();

  return filename;

}

QString AlbumCoverLoader::CoverFilePath(const Song &song, const QString &album_dir, const QUrl &cover_url) {
  return CoverFilePath(song.source(), song.effective_albumartist(), song.album(), song.album_id(), album_dir, cover_url);
}

QString AlbumCoverLoader::CoverFilePath(const Song::Source source, const QString &artist, QString album, const QString &album_id, const QString &album_dir, const QUrl &cover_url) {

  album.remove(Song::kAlbumRemoveDisc);

  QString path;
  if (source == Song::Source_Collection && cover_album_dir_ && !album_dir.isEmpty()) {
    path = album_dir;
  }
  else {
    path = Song::ImageCacheDir(source);
  }

  if (path.right(1) == QDir::separator()) {
    path.chop(1);
  }

  QDir dir;
  if (!dir.mkpath(path)) {
    qLog(Error) << "Unable to create directory" << path;
    path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  }

  QString filename;
  if (source == Song::Source_Collection && cover_album_dir_ && cover_filename_ == CollectionSettingsPage::SaveCover_Pattern && !cover_pattern_.isEmpty()) {
    filename = CoverFilenameFromVariable(artist, album) + ".jpg";
    filename.remove(OrganiseFormat::kInvalidFatCharacters);
    if (cover_lowercase_) filename = filename.toLower();
    if (cover_replace_spaces_) filename.replace(QRegExp("\\s"), "-");
  }
  else {
    filename = CoverFilenameFromSource(source, cover_url, artist, album, album_id);
  }

  QString filepath(path + "/" + filename);

  return filepath;

}

QString AlbumCoverLoader::CoverFilenameFromSource(const Song::Source source, const QUrl &cover_url, const QString &artist, const QString &album, const QString &album_id) {

  QString filename;

  switch (source) {
    case Song::Source_Tidal:
      filename = album_id + "-" + cover_url.fileName();
      break;
    case Song::Source_Subsonic:
    case Song::Source_Qobuz:
      filename = AlbumCoverFilename(artist, album);
      if (filename.length() > 8 && (filename.length() - 5) >= (artist.length() + album.length() - 2)) {
        break;
      }
      // fallthrough
    case Song::Source_Collection:
    case Song::Source_LocalFile:
    case Song::Source_CDDA:
    case Song::Source_Device:
    case Song::Source_Stream:
    case Song::Source_Unknown:
      filename = Utilities::Sha1CoverHash(artist, album).toHex() + ".jpg";
      break;
  }

  return filename;

}

QString AlbumCoverLoader::CoverFilenameFromVariable(const QString &artist, const QString &album) {

  QString filename(cover_pattern_);
  filename.replace("%albumartist", artist);
  filename.replace("%artist", artist);
  filename.replace("%album", album);
  return filename;

}

void AlbumCoverLoader::CancelTask(const quint64 id) {

  QMutexLocker l(&mutex_);
  for (QQueue<Task>::iterator it = tasks_.begin(); it != tasks_.end(); ++it) {
    if (it->id == id) {
      tasks_.erase(it);
      break;
    }
  }

}

void AlbumCoverLoader::CancelTasks(const QSet<quint64> &ids) {

  QMutexLocker l(&mutex_);
  for (QQueue<Task>::iterator it = tasks_.begin(); it != tasks_.end();) {
    if (ids.contains(it->id)) {
      it = tasks_.erase(it);
    }
    else {
      ++it;
    }
  }

}

quint64 AlbumCoverLoader::LoadImageAsync(const AlbumCoverLoaderOptions& options, const Song &song) {
  return LoadImageAsync(options, song.art_automatic(), song.art_manual(), song.url(), song, song.image());
}

quint64 AlbumCoverLoader::LoadImageAsync(const AlbumCoverLoaderOptions &options, const QUrl &art_automatic, const QUrl &art_manual, const QUrl &song_url, const Song song, const QImage &embedded_image) {

  Task task;
  task.options = options;
  task.song = song;
  task.song_url = song_url;
  task.art_manual = art_manual;
  task.art_automatic = art_automatic;
  task.art_updated = false;
  task.embedded_image = embedded_image;
  task.type = AlbumCoverLoaderResult::Type_None;
  task.state = State_Manual;

  {
    QMutexLocker l(&mutex_);
    task.id = next_id_++;
    tasks_.enqueue(task);
  }

  metaObject()->invokeMethod(this, "ProcessTasks", Qt::QueuedConnection);

  return task.id;

}

void AlbumCoverLoader::ProcessTasks() {

  while (!stop_requested_) {
    // Get the next task
    Task task;
    {
      QMutexLocker l(&mutex_);
      if (tasks_.isEmpty()) return;
      task = tasks_.dequeue();
    }

    ProcessTask(&task);
  }

}

void AlbumCoverLoader::ProcessTask(Task *task) {

  TryLoadResult result = TryLoadImage(task);
  if (result.started_async) {
    // The image is being loaded from a remote URL, we'll carry on later when it's done
    return;
  }

  if (result.loaded_success) {
    QPair<QImage, QImage> images = ScaleAndPad(task->options, result.image);
    emit AlbumCoverLoaded(task->id, AlbumCoverLoaderResult(result.type, result.cover_url, result.image, images.first, images.second, task->art_updated));
    return;
  }

  NextState(task);

}

void AlbumCoverLoader::NextState(Task *task) {

  if (task->state == State_Manual) {
    // Try the automatic one next
    task->state = State_Automatic;
    ProcessTask(task);
  }
  else {
    // Give up
    emit AlbumCoverLoaded(task->id, AlbumCoverLoaderResult(AlbumCoverLoaderResult::Type_None, QUrl(), task->options.default_output_image_, task->options.default_output_image_, task->options.default_thumbnail_image_, task->art_updated));
  }

}

AlbumCoverLoader::TryLoadResult AlbumCoverLoader::TryLoadImage(Task *task) {

  // An image embedded in the song itself takes priority
  if (!task->embedded_image.isNull()) {
    QPair<QImage, QImage> images = ScaleAndPad(task->options, task->embedded_image);
    return TryLoadResult(false, true, AlbumCoverLoaderResult::Type_Embedded, QUrl(), images.first);
  }

  // Use cached album cover if possible.
  if (task->state == State_Manual &&
      !task->song.art_manual_is_valid() &&
      task->art_manual.isEmpty() &&
      task->song.source() != Song::Source_Collection &&
      !task->options.scale_output_image_ &&
      !task->options.pad_output_image_) {
    task->song.InitArtManual();
    if (task->song.art_manual_is_valid() && task->art_manual != task->song.art_manual()) {
      task->art_manual = task->song.art_manual();
      task->art_updated = true;
    }
  }

  AlbumCoverLoaderResult::Type type(AlbumCoverLoaderResult::Type_None);
  QUrl cover_url;
  switch (task->state) {
    case State_None:
    case State_Automatic:
      type = AlbumCoverLoaderResult::Type_Automatic;
      cover_url = task->art_automatic;
      break;
    case State_Manual:
      type = AlbumCoverLoaderResult::Type_Manual;
      cover_url = task->art_manual;
      break;
  }
  task->type = type;

  if (!cover_url.isEmpty() && !cover_url.path().isEmpty()) {
    if (cover_url.path() == Song::kManuallyUnsetCover) {
      return TryLoadResult(false, true, AlbumCoverLoaderResult::Type_ManuallyUnset, cover_url, task->options.default_output_image_);
    }
    else if (cover_url.path() == Song::kEmbeddedCover && task->song_url.isLocalFile()) {
      const QImage taglib_image = TagReaderClient::Instance()->LoadEmbeddedArtBlocking(task->song_url.toLocalFile());
      if (!taglib_image.isNull()) {
        return TryLoadResult(false, true, AlbumCoverLoaderResult::Type_Embedded, cover_url, ScaleAndPad(task->options, taglib_image).first);
      }
    }
    else if (cover_url.isLocalFile()) {
      QImage image(cover_url.toLocalFile());
      return TryLoadResult(false, !image.isNull(), type, cover_url, image.isNull() ? task->options.default_output_image_ : image);
    }
    else if (cover_url.scheme().isEmpty()) {  // Assume a local file with no scheme.
      QImage image(cover_url.path());
      return TryLoadResult(false, !image.isNull(), type, cover_url, image.isNull() ? task->options.default_output_image_ : image);
    }
    else if (network_->supportedSchemes().contains(cover_url.scheme())) {  // Remote URL
      QNetworkRequest request(cover_url);
      request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
      QNetworkReply *reply = network_->get(request);
      connect(reply, &QNetworkReply::finished, [=] { RemoteFetchFinished(reply, cover_url); });

      remote_tasks_.insert(reply, *task);
      return TryLoadResult(true, false, type, cover_url, QImage());
    }
  }

  return TryLoadResult(false, false, AlbumCoverLoaderResult::Type_None, cover_url, task->options.default_output_image_);

}

void AlbumCoverLoader::RemoteFetchFinished(QNetworkReply *reply, const QUrl &cover_url) {

  reply->deleteLater();

  if (!remote_tasks_.contains(reply)) return;
  Task task = remote_tasks_.take(reply);

  // Handle redirects.
  QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
  if (redirect.isValid()) {
    if (++task.redirects > kMaxRedirects) {
      return;  // Give up.
    }
    QNetworkRequest request = reply->request();
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setUrl(redirect.toUrl());
    QNetworkReply *redirected_reply = network_->get(request);
    connect(redirected_reply, &QNetworkReply::finished, [=] { RemoteFetchFinished(redirected_reply, redirect.toUrl()); });

    remote_tasks_.insert(redirected_reply, task);
    return;
  }

  if (reply->error() == QNetworkReply::NoError) {
    // Try to load the image
    QImage image;
    if (image.load(reply, 0)) {
      QPair<QImage, QImage> images = ScaleAndPad(task.options, image);
      emit AlbumCoverLoaded(task.id, AlbumCoverLoaderResult(task.type, cover_url, image, images.first, images.second, task.art_updated));
      return;
    }
    else {
      qLog(Error) << "Unable to load album cover image" << cover_url;
    }
  }
  else {
    qLog(Error) << "Unable to get album cover" << cover_url << reply->error() << reply->errorString();
  }

  NextState(&task);

}

QPair<QImage, QImage> AlbumCoverLoader::ScaleAndPad(const AlbumCoverLoaderOptions &options, const QImage &image) {

  if (image.isNull()) return qMakePair(image, image);

  // Scale the image down
  QImage image_scaled;
  if (options.scale_output_image_) {
    image_scaled = image.scaled(QSize(options.desired_height_, options.desired_height_), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  else {
    image_scaled = image;
  }

  // Pad the image to height x height
  if (options.pad_output_image_) {
    QImage image_padded(options.desired_height_, options.desired_height_, QImage::Format_ARGB32);
    image_padded.fill(0);

    QPainter p(&image_padded);
    p.drawImage((options.desired_height_ - image_scaled.width()) / 2, (options.desired_height_ - image_scaled.height()) / 2, image_scaled);
    p.end();

    image_scaled = image_padded;
  }

  // Create thumbnail
  QImage image_thumbnail;
  if (options.create_thumbnail_) {
    if (options.pad_thumbnail_image_) {
      image_thumbnail = image.scaled(options.thumbnail_size_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      QImage image_padded(options.thumbnail_size_, QImage::Format_ARGB32_Premultiplied);
      image_padded.fill(0);

      QPainter p(&image_padded);
      p.drawImage((image_padded.width() - image_thumbnail.width()) / 2, (image_padded.height() - image_thumbnail.height()) / 2, image_thumbnail);
      p.end();

      image_thumbnail = image_padded;
    }
    else {
      image_thumbnail = image.scaledToHeight(options.thumbnail_size_.height(), Qt::SmoothTransformation);
    }
  }

  return qMakePair(image_scaled, image_thumbnail);

}

QPixmap AlbumCoverLoader::TryLoadPixmap(const QUrl &art_automatic, const QUrl &art_manual, const QUrl &url) {

  QPixmap ret;

  if (!art_manual.path().isEmpty()) {
    if (art_manual.path() == Song::kManuallyUnsetCover) return ret;
    else if (art_manual.isLocalFile()) {
      ret.load(art_manual.toLocalFile());
    }
    else if (art_manual.scheme().isEmpty()) {
      ret.load(art_manual.path());
    }
  }
  if (ret.isNull() && !art_automatic.path().isEmpty()) {
    if (art_automatic.path() == Song::kEmbeddedCover && !url.isEmpty() && url.isLocalFile()) {
      ret = QPixmap::fromImage(TagReaderClient::Instance()->LoadEmbeddedArtBlocking(url.toLocalFile()));
    }
    else if (art_automatic.isLocalFile()) {
      ret.load(art_automatic.toLocalFile());
    }
    else if (art_automatic.scheme().isEmpty()) {
      ret.load(art_automatic.path());
    }
  }

  return ret;

}
