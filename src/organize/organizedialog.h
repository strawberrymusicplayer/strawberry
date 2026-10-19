/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ORGANISEDIALOG_H
#define ORGANISEDIALOG_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QDialog>
#include <QFuture>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QPointer>
#include <QtEvents>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "organize.h"
#include "organizeformat.h"

class QAbstractItemModel;
class QWidget;
class QResizeEvent;
class QShowEvent;
class QCloseEvent;

class TaskManager;
class CollectionBackend;
class CollectionDirectoryModel;
class OrganizeErrorDialog;
class Ui_OrganizeDialog;

class OrganizeDialog : public QDialog {
  Q_OBJECT

 public:
  explicit OrganizeDialog(const SharedPtr<TaskManager> task_manager,
                          const SharedPtr<TagReaderClient> tagreader_client,
                          const SharedPtr<CollectionBackend> collection_backend = nullptr,
                          QWidget *parentwindow = nullptr,
                          QWidget *parent = nullptr);

  ~OrganizeDialog() override;

  void SetDestinationModel(QAbstractItemModel *model, const bool devices = false);

  // Empty directories left by moving songs are only removed inside the collection directories, the source roots, and the destination.
  // The collection directories are used for every source, so songs moved from the collection to a device are also cleaned up.
  void SetCollectionDirectoryModel(CollectionDirectoryModel *collection_directory_model);
  // The device directories of the songs, when they are not in the collection.
  void SetSourceRoots(const QStringList &source_roots);

  // These functions return true if any songs were actually added to the dialog.
  // SetSongs returns immediately, SetUrls and SetFilenames load the songs in the background.
  bool SetSongs(const SongList &songs);
  bool SetUrls(const QList<QUrl> &urls);
  bool SetFilenames(const QStringList &filenames);

  void SetCopy(const bool copy);

  // root is the destination directory, to keep the current files of the songs in it from being overwritten by other songs.
  static Organize::NewSongInfoList ComputeNewSongsFilenames(const SongList &songs, const OrganizeFormat &format, const QString &extension = QString(), const QString &root = QString());

  void SetPlaylist(const QString &playlist);

 protected:
  void showEvent(QShowEvent *e) override;
  void closeEvent(QCloseEvent *e) override;

 private:
  void LoadGeometry();
  void SaveGeometry();
  void LoadSettings();
  void AdjustSize();

  SongList LoadSongsBlocking(const QStringList &filenames) const;
  void SetLoadingSongs(const bool loading);

 Q_SIGNALS:
  void FileCopied(const int);

 public Q_SLOTS:
  void accept() override;
  void reject() override;

 private Q_SLOTS:
  void SaveSettings();
  void RestoreDefaults();

  void InsertTag(const QString &tag);
  void UpdatePreviews();

  void OrganizeFinished(const QStringList &files_with_errors, const QStringList &log, const bool success);

  void AllowExtASCII(const bool checked);

 private:
  QWidget *parentwindow_;
  Ui_OrganizeDialog *ui_;

  const SharedPtr<TaskManager> task_manager_;
  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<CollectionBackend> collection_backend_;

  OrganizeFormat format_;

  QFuture<SongList> songs_future_;
  SongList songs_;
  Organize::NewSongInfoList new_songs_info_;
  quint64 total_size_;
  QString playlist_;

  ScopedPtr<OrganizeErrorDialog> error_dialog_;

  bool devices_;

  QPointer<CollectionDirectoryModel> collection_directory_model_;
  QStringList source_roots_;
};

#endif  // ORGANISEDIALOG_H
