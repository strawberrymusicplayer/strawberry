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
#include <QList>
#include <QQueue>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QSize>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

#include "core/closure.h"
#include "core/network.h"
#include "core/song.h"
#include "core/tagreaderclient.h"
#include "core/utilities.h"
#include "settings/collectionsettingspage.h"
#include "organise/organiseformat.h"
#include "albumcoverloader.h"
#include "albumcoverloaderoptions.h"

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

QString AlbumCoverLoader::ImageCacheDir(const Song::Source source) {

  switch (source) {
    case Song::Source_LocalFile:
    case Song::Source_Collection:
    case Song::Source_CDDA:
    case Song::Source_Device:
    case Song::Source_Stream:
    case Song::Source_Unknown:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/albumcovers";
    case Song::Source_Tidal:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tidalalbumcovers";
    case Song::Source_Qobuz:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/qobuzalbumcovers";
    case Song::Source_Subsonic:
      return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/subsonicalbumcovers";
  }

  return QString();

}

QString AlbumCoverLoader::CoverFilePath(const Song::Source source, const QString &artist, QString album, const QString &album_id, const QString &album_dir, const QUrl &cover_url) {

  album.remove(Song::kAlbumRemoveDisc);

  QString path;
  if (source == Song::Source_Collection && cover_album_dir_ && !album_dir.isEmpty()) {
    path = album_dir;
  }
  else {
    path = AlbumCoverLoader::ImageCacheDir(source);
  }

  if (path.right(1) == QDir::separator()) {
    path.chop(1);
  }

  QDir dir;
  if (!dir.mkpath(path)) {
    qLog(Error) << "Unable to create directory" << path;
    return QString();
  }

  QString filename;
  if (source == Song::Source_Collection && cover_album_dir_ && cover_filename_ == CollectionSettingsPage::SaveCover_Pattern && !cover_pattern_.isEmpty()) {
    filename = CreateCoverFilename(artist, album) + ".jpg";
    filename.remove(OrganiseFormat::kValidFatCharacters);
    if (cover_lowercase_) filename = filename.toLower();
    if (cover_replace_spaces_) filename.replace(QRegExp("\\s"), "-");
  }
  else {
    switch (source) {
      case Song::Source_Collection:
      case Song::Source_LocalFile:
      case Song::Source_CDDA:
      case Song::Source_Device:
      case Song::Source_Stream:
      case Song::Source_Unknown:
        filename = Utilities::Sha1CoverHash(artist, album).toHex() + ".jpg";
        break;
      case Song::Source_Tidal:
        filename = album_id + "-" + cover_url.fileName();
        break;
      case Song::Source_Qobuz:
      case Song::Source_Subsonic:
        filename = AlbumCoverFileName(artist, album);
        break;
    }
  }

  if (filename.isEmpty()) return QString();

  QString filepath(path + "/" + filename);

  return filepath;

}

QString AlbumCoverLoader::AlbumCoverFileName(QString artist, QString album) {

  artist.remove('/');
  album.remove('/');

  QString filename = artist + "-" + album + ".jpg";
  filename = Utilities::UnicodeToAscii(filename.toLower());
  filename.replace(' ', '-');
  filename.replace("--", "-");
  filename.remove(OrganiseFormat::kValidFatCharacters);

  return filename;

}

QString AlbumCoverLoader::CreateCoverFilename(const QString &artist, const QString &album) {

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
  return LoadImageAsync(options, song.art_automatic(), song.art_manual(), song.url().toLocalFile(), song.image());
}

quint64 AlbumCoverLoader::LoadImageAsync(const AlbumCoverLoaderOptions &options, const QUrl &art_automatic, const QUrl &art_manual, const QString &song_filename, const QImage &embedded_image) {

  Task task;
  task.options = options;
  task.art_automatic = art_automatic;
  task.art_manual = art_manual;
  task.song_filename = song_filename;
  task.embedded_image = embedded_image;
  task.state = State_TryingManual;

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

  TryLoadResult result = TryLoadImage(*task);
  if (result.started_async) {
    // The image is being loaded from a remote URL, we'll carry on later when it's done
    return;
  }

  if (result.loaded_success) {
    QImage scaled = ScaleAndPad(task->options, result.image);
    emit ImageLoaded(task->id, result.cover_url, scaled);
    emit ImageLoaded(task->id, result.cover_url, scaled, result.image);
    return;
  }

  NextState(task);

}

void AlbumCoverLoader::NextState(Task *task) {

  if (task->state == State_TryingManual) {
    // Try the automatic one next
    task->state = State_TryingAuto;
    ProcessTask(task);
  }
  else {
    // Give up
    emit ImageLoaded(task->id, QUrl(), task->options.default_output_image_);
    emit ImageLoaded(task->id, QUrl(), task->options.default_output_image_, task->options.default_output_image_);
  }

}

AlbumCoverLoader::TryLoadResult AlbumCoverLoader::TryLoadImage(const Task &task) {

  // An image embedded in the song itself takes priority
  if (!task.embedded_image.isNull())
    return TryLoadResult(false, true, QUrl(), ScaleAndPad(task.options, task.embedded_image));

  QUrl cover_url;

  switch (task.state) {
    case State_TryingAuto:   cover_url = task.art_automatic; break;
    case State_TryingManual: cover_url = task.art_manual;    break;
  }

  if (cover_url.path() == Song::kManuallyUnsetCover)
    return TryLoadResult(false, true, QUrl(), task.options.default_output_image_);

  else if (cover_url.path() == Song::kEmbeddedCover && !task.song_filename.isEmpty()) {
    const QImage taglib_image = TagReaderClient::Instance()->LoadEmbeddedArtBlocking(task.song_filename);

    if (!taglib_image.isNull())
      return TryLoadResult(false, true, QUrl(), ScaleAndPad(task.options, taglib_image));
  }

  if (cover_url.path().isEmpty()) {
    return TryLoadResult(false, false, cover_url, task.options.default_output_image_);
  }
  else {
    if (cover_url.isLocalFile()) {
      QImage image(cover_url.toLocalFile());
      return TryLoadResult(false, !image.isNull(), cover_url, image.isNull() ? task.options.default_output_image_ : image);
    }
    else if (cover_url.scheme().isEmpty()) {  // Assume a local file with no scheme.
      QImage image(cover_url.path());
      return TryLoadResult(false, !image.isNull(), cover_url, image.isNull() ? task.options.default_output_image_ : image);
    }
    else if (network_->supportedSchemes().contains(cover_url.scheme())) {  // Remote URL
      QNetworkReply *reply = network_->get(QNetworkRequest(cover_url));
      NewClosure(reply, SIGNAL(finished()), this, SLOT(RemoteFetchFinished(QNetworkReply*, const QUrl&)), reply, cover_url);

      remote_tasks_.insert(reply, task);
      return TryLoadResult(true, false, cover_url, QImage());
    }
  }

  return TryLoadResult(false, false, cover_url, task.options.default_output_image_);

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
    request.setUrl(redirect.toUrl());
    QNetworkReply *redirected_reply = network_->get(request);
    NewClosure(redirected_reply, SIGNAL(finished()), this, SLOT(RemoteFetchFinished(QNetworkReply*, const QUrl&)), redirected_reply, redirect.toUrl());

    remote_tasks_.insert(redirected_reply, task);
    return;
  }

  if (reply->error() == QNetworkReply::NoError) {
    // Try to load the image
    QImage image;
    if (image.load(reply, 0)) {
      QImage scaled = ScaleAndPad(task.options, image);
      emit ImageLoaded(task.id, cover_url, scaled);
      emit ImageLoaded(task.id, cover_url, scaled, image);
      return;
    }
  }

  NextState(&task);

}

QImage AlbumCoverLoader::ScaleAndPad(const AlbumCoverLoaderOptions &options, const QImage &image) {

  if (image.isNull()) return image;

  // Scale the image down
  QImage copy;
  if (options.scale_output_image_) {
    copy = image.scaled(QSize(options.desired_height_, options.desired_height_), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  else {
    copy = image;
  }

  if (!options.pad_output_image_) return copy;

  // Pad the image to height_ x height_
  QImage padded_image(options.desired_height_, options.desired_height_, QImage::Format_ARGB32);
  padded_image.fill(0);

  QPainter p(&padded_image);
  p.drawImage((options.desired_height_ - copy.width()) / 2, (options.desired_height_ - copy.height()) / 2, copy);
  p.end();

  return padded_image;

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
