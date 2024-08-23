/* This file was part of Clementine.
   Copyright 2012, David Sansome <me@davidsansome.com>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "moodbarloader.h"

#include <memory>
#include <chrono>

#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QIODevice>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QAbstractNetworkCache>
#include <QNetworkDiskCache>
#include <QTimer>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QSettings>

#include "core/logging.h"
#include "core/scoped_ptr.h"
#include "core/application.h"
#include "core/settings.h"

#include "moodbarpipeline.h"

#include "settings/moodbarsettingspage.h"

using namespace std::chrono_literals;

#ifdef Q_OS_WIN32
#  include <windows.h>
#endif

MoodbarLoader::MoodbarLoader(Application *app, QObject *parent)
    : QObject(parent),
      cache_(new QNetworkDiskCache(this)),
      thread_(new QThread(this)),
      kMaxActiveRequests(qMax(1, QThread::idealThreadCount() / 2)),
      save_(false) {

  cache_->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/moodbar"));
  cache_->setMaximumCacheSize(60LL * 1024LL * 1024LL);  // 60MB - enough for 20,000 moodbars

  QObject::connect(app, &Application::SettingsChanged, this, &MoodbarLoader::ReloadSettings);
  ReloadSettings();

}

MoodbarLoader::~MoodbarLoader() {
  thread_->quit();
  thread_->wait(1000);
}

void MoodbarLoader::ReloadSettings() {

  Settings s;
  s.beginGroup(MoodbarSettingsPage::kSettingsGroup);
  save_ = s.value("save", false).toBool();
  s.endGroup();

  MaybeTakeNextRequest();

}

QStringList MoodbarLoader::MoodFilenames(const QString &song_filename) {

  const QFileInfo file_info(song_filename);
  const QString dir_path(file_info.dir().path());
  const QString mood_filename = file_info.completeBaseName() + QStringLiteral(".mood");

  return QStringList() << dir_path + QStringLiteral("/.") + mood_filename << dir_path + QLatin1Char('/') + mood_filename;

}

QUrl MoodbarLoader::CacheUrlEntry(const QString &filename) {

  return QUrl(QString::fromLatin1(QUrl::toPercentEncoding(filename)));

}

MoodbarLoader::Result MoodbarLoader::Load(const QUrl &url, const bool has_cue, QByteArray *data, MoodbarPipeline **async_pipeline) {

  if (!url.isLocalFile() || has_cue) {
    return Result::CannotLoad;
  }

  // Are we in the middle of loading this moodbar already?
  if (requests_.contains(url)) {
    *async_pipeline = requests_.value(url);
    return Result::WillLoadAsync;
  }

  // Check if a mood file exists for this file already
  const QString filename(url.toLocalFile());

  const QStringList possible_mood_files = MoodFilenames(filename);
  for (const QString &possible_mood_file : possible_mood_files) {
    QFile f(possible_mood_file);
    if (f.exists()) {
      if (f.open(QIODevice::ReadOnly)) {
        qLog(Info) << "Loading moodbar data from" << possible_mood_file;
        *data = f.readAll();
        f.close();
        return Result::Loaded;
      }
      else {
        qLog(Error) << "Failed to load moodbar data from" << possible_mood_file << f.errorString();
      }
    }
  }

  // Maybe it exists in the cache?

  QNetworkCacheMetaData disk_cache_metadata = cache_->metaData(CacheUrlEntry(filename));
  if (disk_cache_metadata.isValid()) {
    ScopedPtr<QIODevice> device_cache_file(cache_->data(disk_cache_metadata.url()));
    if (device_cache_file) {
      qLog(Info) << "Loading cached moodbar data for" << filename;
      *data = device_cache_file->readAll();
      if (!data->isEmpty()) {
        return Result::Loaded;
      }
    }
  }

  if (!thread_->isRunning()) thread_->start(QThread::IdlePriority);

  // There was no existing file, analyze the audio file and create one.
  MoodbarPipeline *pipeline = new MoodbarPipeline(url);
  pipeline->moveToThread(thread_);
  QObject::connect(pipeline, &MoodbarPipeline::Finished, this, [this, pipeline, url]() { RequestFinished(pipeline, url); });

  requests_[url] = pipeline;
  queued_requests_ << url;

  MaybeTakeNextRequest();

  *async_pipeline = pipeline;
  return Result::WillLoadAsync;

}

void MoodbarLoader::MaybeTakeNextRequest() {

  Q_ASSERT(QThread::currentThread() == qApp->thread());

  if (active_requests_.count() >= kMaxActiveRequests || queued_requests_.isEmpty()) {
    return;
  }

  const QUrl url = queued_requests_.takeFirst();
  active_requests_ << url;

  qLog(Info) << "Creating moodbar data for" << url.toLocalFile();
  QMetaObject::invokeMethod(requests_.value(url), &MoodbarPipeline::Start, Qt::QueuedConnection);

}

void MoodbarLoader::RequestFinished(MoodbarPipeline *request, const QUrl &url) {

  Q_ASSERT(QThread::currentThread() == qApp->thread());

  if (request->success()) {

    const QString filename = url.toLocalFile();

    qLog(Info) << "Moodbar data generated successfully for" << filename;

    // Save the data in the cache
    QNetworkCacheMetaData disk_cache_metadata;
    disk_cache_metadata.setSaveToDisk(true);
    disk_cache_metadata.setUrl(CacheUrlEntry(filename));
    // Qt 6 now ignores any entry without headers, so add a fake header.
    disk_cache_metadata.setRawHeaders(QNetworkCacheMetaData::RawHeaderList() << qMakePair(QByteArray("moodbar"), QByteArray("moodbar")));

    QIODevice *device_cache_file = cache_->prepare(disk_cache_metadata);
    if (device_cache_file) {
      const qint64 data_written = device_cache_file->write(request->data());
      if (data_written > 0) {
        cache_->insert(device_cache_file);
      }
    }

    // Save the data alongside the original as well if we're configured to.
    if (save_) {
      QStringList mood_filenames = MoodFilenames(url.toLocalFile());
      const QString mood_filename(mood_filenames[0]);
      QFile mood_file(mood_filename);
      if (mood_file.open(QIODevice::WriteOnly)) {
        if (mood_file.write(request->data()) <= 0) {
          qLog(Error) << "Error writing to mood file" << mood_filename << mood_file.errorString();
        }
        mood_file.close();
#ifdef Q_OS_WIN32
        if (!SetFileAttributes(reinterpret_cast<LPCTSTR>(mood_filename.utf16()), FILE_ATTRIBUTE_HIDDEN)) {
          qLog(Warning) << "Error setting hidden attribute for file" << mood_filename;
        }
#endif
      }
      else {
        qLog(Error) << "Error opening mood file" << mood_filename << "for writing:" << mood_file.errorString();
      }
    }
  }

  // Remove the request from the active list and delete it
  requests_.remove(url);
  active_requests_.remove(url);

  QTimer::singleShot(1s, request, &MoodbarLoader::deleteLater);

  MaybeTakeNextRequest();

}
