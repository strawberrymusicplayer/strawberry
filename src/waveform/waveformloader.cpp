/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#include "waveformloader.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QCoreApplication>
#include <QIODevice>
#include <QAbstractNetworkCache>
#include <QNetworkDiskCache>
#include <QByteArray>
#include <QString>
#include <QUrl>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/standardpaths.h"

#include "waveformbuilder.h"
#include "waveformpipeline.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

WaveformLoader::WaveformLoader(QObject *parent)
    : QObject(parent),
      cache_(new QNetworkDiskCache(this)),
      thread_(new QThread(this)),
      kMaxActiveRequests(qMax(1, QThread::idealThreadCount() / 2)) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));
  thread_->setObjectName(objectName());

  cache_->setCacheDirectory(StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + u"/waveform"_s);
  cache_->setMaximumCacheSize(60LL * 1024LL * 1024LL);  // 60MB

}

WaveformLoader::~WaveformLoader() {
  thread_->quit();
  thread_->wait(1000);
}

QUrl WaveformLoader::CacheUrlEntry(const QString &filename) {

  return QUrl(QString::fromLatin1(QUrl::toPercentEncoding(filename)));

}

WaveformLoader::LoadResult WaveformLoader::Load(const QUrl &url, const bool has_cue) {

  if (!url.isLocalFile() || has_cue) {
    return LoadStatus::CannotLoad;
  }

  // Are we in the middle of loading this waveform already?
  if (requests_.contains(url)) {
    return LoadResult(LoadStatus::WillLoadAsync, requests_.value(url));
  }

  const QString filename(url.toLocalFile());

  // Maybe it exists in the cache?

  QNetworkCacheMetaData disk_cache_metadata = cache_->metaData(CacheUrlEntry(filename));
  if (disk_cache_metadata.isValid()) {
    ScopedPtr<QIODevice> device_cache_file(cache_->data(disk_cache_metadata.url()));
    if (device_cache_file) {
      qLog(Debug) << "Loading cached waveform data for" << filename;
      const QByteArray data = device_cache_file->readAll();
      // Validate the blob (magic, version, declared length) before handing it to
      // the consumer. A truncated, corrupt or future-version entry is treated as
      // a cache miss and regenerated rather than deserialized into garbage.
      if (WaveformBuilder::IsValidBlob(data)) {
        return LoadResult(LoadStatus::Loaded, data);
      }
      qLog(Warning) << "Discarding invalid cached waveform data for" << filename;
    }
  }

  if (!thread_->isRunning()) thread_->start(QThread::IdlePriority);

  // There was no existing data, analyze the audio file and create one.
  WaveformPipelinePtr pipeline = WaveformPipelinePtr(new WaveformPipeline(url));
  pipeline->moveToThread(thread_);
  SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
  *connection = QObject::connect(&*pipeline, &WaveformPipeline::Finished, this, [this, connection, pipeline, url]() {
    RequestFinished(pipeline, url);
    QObject::disconnect(*connection);
  });

  requests_[url] = pipeline;
  queued_requests_ << url;

  MaybeTakeNextRequest();

  return LoadResult(LoadStatus::WillLoadAsync, pipeline);

}

void WaveformLoader::MaybeTakeNextRequest() {

  Q_ASSERT(QThread::currentThread() == qApp->thread());

  if (active_requests_.count() >= kMaxActiveRequests || queued_requests_.isEmpty()) {
    return;
  }

  const QUrl url = queued_requests_.takeFirst();
  active_requests_ << url;

  qLog(Debug) << "Creating waveform data for" << url.toLocalFile();

  WaveformPipelinePtr pipeline = requests_.value(url);
  QMetaObject::invokeMethod(&*pipeline, &WaveformPipeline::Start, Qt::QueuedConnection);

}

void WaveformLoader::RequestFinished(WaveformPipelinePtr pipeline, const QUrl &url) {

  Q_ASSERT(QThread::currentThread() == qApp->thread());

  if (pipeline->success()) {

    const QString filename = url.toLocalFile();

    qLog(Debug) << "Waveform data generated successfully for" << filename;

    // Save the data in the cache
    QNetworkCacheMetaData disk_cache_metadata;
    disk_cache_metadata.setSaveToDisk(true);
    disk_cache_metadata.setUrl(CacheUrlEntry(filename));
    // Qt 6 now ignores any entry without headers, so add a fake header.
    disk_cache_metadata.setRawHeaders(QNetworkCacheMetaData::RawHeaderList() << qMakePair(QByteArray("waveform"), QByteArray("waveform")));

    QIODevice *device_cache_file = cache_->prepare(disk_cache_metadata);
    if (device_cache_file) {
      const qint64 data_written = device_cache_file->write(pipeline->data());
      if (data_written > 0) {
        cache_->insert(device_cache_file);
      }
    }
  }

  // Remove the request from the active list and delete it
  requests_.remove(url);
  active_requests_.remove(url);

  MaybeTakeNextRequest();

}
