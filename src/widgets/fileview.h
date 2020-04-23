/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef FILEVIEW_H
#define FILEVIEW_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QWidget>
#include <QAbstractItemModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUndoCommand>

#include "core/song.h"

class QMimeData;
class QFileSystemModel;
class QUndoStack;
class QKeyEvent;
class QShowEvent;

class MusicStorage;
class TaskManager;
class Ui_FileView;

class FileView : public QWidget {
  Q_OBJECT

 public:
  explicit FileView(QWidget *parent = nullptr);
  ~FileView();

  static const char *kFileFilter;

  void SetPath(const QString &path);
  void SetTaskManager(TaskManager *task_manager);

  void showEvent(QShowEvent*);
  void keyPressEvent(QKeyEvent *e);

 signals:
  void PathChanged(const QString &path);

  void AddToPlaylist(QMimeData *data);
  void CopyToCollection(const QList<QUrl> &urls);
  void MoveToCollection(const QList<QUrl> &urls);
  void CopyToDevice(const QList<QUrl> &urls);
  void EditTags(const QList<QUrl> &urls);

 private slots:
  void FileUp();
  void FileHome();
  void ChangeFilePath(const QString &new_path);
  void ItemActivated(const QModelIndex &idx);
  void ItemDoubleClick(const QModelIndex &idx);

  void Delete(const QStringList &filenames);
  void DeleteFinished(const SongList &songs_with_errors);

 private:
  void ChangeFilePathWithoutUndo(const QString &new_path);

 private:
  class UndoCommand : public QUndoCommand {
   public:
    UndoCommand(FileView *view, const QString &new_path);

    QString undo_path() const { return old_state_.path; }

    void undo();
    void redo();

   private:
    struct State {
      State() : scroll_pos(-1) {}

      QString path;
      QModelIndex index;
      int scroll_pos;
    };

    FileView *view_;
    State old_state_;
    State new_state_;
  };

  Ui_FileView *ui_;

  QFileSystemModel *model_;
  QUndoStack *undo_stack_;

  TaskManager *task_manager_;
  std::shared_ptr<MusicStorage> storage_;

  QString lazy_set_path_;

  QStringList filter_list_;
};

#endif  // FILEVIEW_H
