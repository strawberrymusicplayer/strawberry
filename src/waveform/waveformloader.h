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

#ifndef WAVEFORMLOADER_H
#define WAVEFORMLOADER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "waveformpipeline.h"

class QThread;
class QByteArray;
class QNetworkDiskCache;

// Async orchestrator for track waveform data. Owns a worker QThread (idle I/O
// priority) that runs WaveformPipeline decodes, caps in-flight requests at
// idealThreadCount/2, dedupes in-flight requests by URL, and persists generated
// blobs to a QNetworkDiskCache at CacheLocation/waveform keyed by the
// percent-encoded source path. Replaying a track returns the cached blob
// synchronously instead of re-decoding.
class WaveformLoader : public QObject {
  Q_OBJECT

 public:
  explicit WaveformLoader(QObject *parent = nullptr);
  ~WaveformLoader() override;

  enum class LoadStatus {
    // The URL isn't a local file - waveform data can never be loaded.
    CannotLoad,

    // Waveform data was loaded and returned.
    Loaded,

    // Waveform data will be loaded in the background, a WaveformPipeline
    // was returned that you can connect to the Finished() signal on.
    WillLoadAsync
  };

  class LoadResult {
   public:
    LoadResult(const LoadStatus _status) : status(_status) {}
    LoadResult(const LoadStatus _status, WaveformPipelinePtr _pipeline) : status(_status), pipeline(_pipeline) {}
    LoadResult(const LoadStatus _status, const QByteArray &_data) : status(_status), data(_data) {}
    LoadStatus status;
    WaveformPipelinePtr pipeline;
    QByteArray data;
  };

  LoadResult Load(const QUrl &url, const bool has_cue);

  void ReloadSettings();

  static QStringList WaveformFilenames(const QString &song_filename);

 Q_SIGNALS:
  void SettingsReloaded();

 private:
  static QUrl CacheUrlEntry(const QString &filename);
  void RequestFinished(WaveformPipelinePtr pipeline, const QUrl &url);
  void MaybeTakeNextRequest();

 private:
  QNetworkDiskCache *cache_;
  QThread *thread_;

  const int kMaxActiveRequests;

  bool save_;

  QMap<QUrl, WaveformPipelinePtr> requests_;
  QList<QUrl> queued_requests_;
  QSet<QUrl> active_requests_;
};

#endif  // WAVEFORMLOADER_H
