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

#ifndef ALBUMCOVERLOADER_H
#define ALBUMCOVERLOADER_H

#include "config.h"


#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QMap>
#include <QSet>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QNetworkReply>

#include "core/song.h"
#include "settings/collectionsettingspage.h"
#include "albumcoverloaderoptions.h"

class QThread;
class Song;
class NetworkAccessManager;

class AlbumCoverLoader : public QObject {
  Q_OBJECT

 public:
  explicit AlbumCoverLoader(QObject *parent = nullptr);

  void ReloadSettings();

  void ExitAsync();
  void Stop() { stop_requested_ = true; }

  static QString ImageCacheDir(const Song::Source source);
  QString CreateCoverFilename(const QString &artist, const QString &album);
  QString CoverFilePath(const Song::Source source, const QString &artist, QString album, const QString &album_id, const QString &album_dir, const QUrl &cover_url);
  QString AlbumCoverFileName(QString artist, QString album);

  quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const Song &song);
  virtual quint64 LoadImageAsync(const AlbumCoverLoaderOptions &options, const QUrl &art_automatic, const QUrl &art_manual, const QString &song_filename = QString(), const QImage &embedded_image = QImage());

  void CancelTask(const quint64 id);
  void CancelTasks(const QSet<quint64> &ids);

  static QPixmap TryLoadPixmap(const QUrl &automatic, const QUrl &manual, const QUrl &url = QUrl());
  static QImage ScaleAndPad(const AlbumCoverLoaderOptions &options, const QImage &image);

 signals:
  void ExitFinished();
  void ImageLoaded(const quint64 id, const QUrl &cover_url, const QImage &image);
  void ImageLoaded(const quint64 id, const QUrl &cover_url, const QImage &scaled, const QImage &original);

 protected slots:
  void Exit();
  void ProcessTasks();
  void RemoteFetchFinished(QNetworkReply *reply, const QUrl &cover_url);

 protected:
  enum State {
    State_TryingManual,
    State_TryingAuto,
  };

  struct Task {
    Task() : redirects(0) {}

    AlbumCoverLoaderOptions options;

    quint64 id;
    QUrl art_automatic;
    QUrl art_manual;
    QString song_filename;
    QImage embedded_image;
    State state;
    int redirects;
  };

  struct TryLoadResult {
    TryLoadResult(bool async, bool success, const QUrl &_cover_url, const QImage &_image) : started_async(async), loaded_success(success), cover_url(_cover_url), image(_image) {}

    bool started_async;
    bool loaded_success;

    QUrl cover_url;
    QImage image;
  };

  void ProcessTask(Task *task);
  void NextState(Task *task);
  TryLoadResult TryLoadImage(const Task &task);

  bool stop_requested_;

  QMutex mutex_;
  QQueue<Task> tasks_;
  QMap<QNetworkReply *, Task> remote_tasks_;
  quint64 next_id_;

  NetworkAccessManager *network_;

  static const int kMaxRedirects = 3;

  bool cover_album_dir_;
  CollectionSettingsPage::SaveCover cover_filename_;
  QString cover_pattern_;
  bool cover_overwrite_;
  bool cover_lowercase_;
  bool cover_replace_spaces_;

  QThread *original_thread_;

};

#endif  // ALBUMCOVERLOADER_H

