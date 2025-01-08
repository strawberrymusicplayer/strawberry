/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MOODBARLOADER_H
#define MOODBARLOADER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "moodbarpipeline.h"

class QThread;
class QByteArray;
class QNetworkDiskCache;

class MoodbarLoader : public QObject {
  Q_OBJECT

 public:
  explicit MoodbarLoader(QObject *parent = nullptr);
  ~MoodbarLoader() override;

  enum class LoadStatus {
    // The URL isn't a local file or the moodbar plugin was not available -
    // moodbar data can never be loaded.
    CannotLoad,

    // Moodbar data was loaded and returned.
    Loaded,

    // Moodbar data will be loaded in the background, a MoodbarPipeline
    // was returned that you can connect to the Finished() signal on.
    WillLoadAsync
  };

  class LoadResult {
   public:
    LoadResult(const LoadStatus _status) : status(_status) {}
    LoadResult(const LoadStatus _status, MoodbarPipelinePtr _pipeline) : status(_status), pipeline(_pipeline) {}
    LoadResult(const LoadStatus _status, const QByteArray &_data) : status(_status), data(_data) {}
    LoadStatus status;
    MoodbarPipelinePtr pipeline;
    QByteArray data;
  };

  void ReloadSettings();

  LoadResult Load(const QUrl &url, const bool has_cue);

 private:
  static QStringList MoodFilenames(const QString &song_filename);
  static QUrl CacheUrlEntry(const QString &filename);
  void RequestFinished(MoodbarPipelinePtr pipeline, const QUrl &url);
  void MaybeTakeNextRequest();

 Q_SIGNALS:
  void MoodbarEnabled(const bool enabled);
  void StyleChanged();
  void SettingsReloaded();

 private:
  QNetworkDiskCache *cache_;
  QThread *thread_;

  const int kMaxActiveRequests;

  QMap<QUrl, MoodbarPipelinePtr> requests_;
  QList<QUrl> queued_requests_;
  QSet<QUrl> active_requests_;

  bool save_;
};

#endif  // MOODBARLOADER_H
