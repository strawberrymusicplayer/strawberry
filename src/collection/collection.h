/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTION_H
#define COLLECTION_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QHash>
#include <QString>
#include <QThread>

#include "core/song.h"
#include "utilities/threadutils.h"

class Application;
class Thread;
class CollectionBackend;
class CollectionModel;
class CollectionWatcher;

class SCollection : public QObject {
  Q_OBJECT

 public:
  explicit SCollection(Application *app, QObject *parent);
  ~SCollection() override;

  static const char *kSongsTable;
  static const char *kFtsTable;
  static const char *kDirsTable;
  static const char *kSubdirsTable;

  void Init();
  void Exit();

  CollectionBackend *backend() const { return backend_; }
  CollectionModel *model() const { return model_; }

  QString full_rescan_reason(int schema_version) const { return full_rescan_revisions_.value(schema_version, QString()); }

  void SyncPlaycountAndRatingToFilesAsync();

 private:
  void SyncPlaycountAndRatingToFiles();

 public slots:
  void ReloadSettings();

  void PauseWatcher();
  void ResumeWatcher();

  void FullScan();
  void AbortScan();
  void Rescan(const SongList &songs);

  void IncrementalScan();

 private slots:
  void ExitReceived();
  void SongsPlaycountChanged(const SongList &songs, const bool save_tags = false);
  void SongsRatingChanged(const SongList &songs, const bool save_tags = false);

 signals:
  void Error(QString);
  void ExitFinished();

 private:
  Application *app_;
  CollectionBackend *backend_;
  CollectionModel *model_;

  CollectionWatcher *watcher_;
  Thread *watcher_thread_;
  QThread *original_thread_;

  // DB schema versions which should trigger a full collection rescan (each of those with a short reason why).
  QHash<int, QString> full_rescan_revisions_;

  QList<QObject*> wait_for_exit_;

  Utilities::IoPriority io_priority_;
  QThread::Priority thread_priority_;
  bool save_playcounts_to_files_;
  bool save_ratings_to_files_;
};

#endif
