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

#include "config.h"

#include <QWidget>
#include <QUndoStack>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSettings>
#include <QMessageBox>
#include <QScrollBar>
#include <QLineEdit>
#include <QToolButton>
#include <QtEvents>

#include "core/deletefiles.h"
#include "core/filesystemmusicstorage.h"
#include "core/iconloader.h"
#include "core/mimedata.h"
#include "fileview.h"
#include "fileviewlist.h"
#include "ui_fileview.h"
#ifdef HAVE_GSTREAMER
#  include "organise/organiseerrordialog.h"
#endif
#include "settings/appearancesettingspage.h"

const char *FileView::kFileFilter =
    "*.wav *.flac *.wv *.ogg *.oga *.opus *.spx *.ape *.mpc "
    "*.mp2 *.mp3 *.m4a *.mp4 *.aac *.asf *.asx *.wma "
    "*.aif *.aiff *.mka *.tta *.dsf *.dsd "
    "*.cue *.m3u *.m3u8 *.pls *.xspf *.asxini ";

FileView::FileView(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_FileView),
      model_(nullptr),
      undo_stack_(new QUndoStack(this)),
      task_manager_(nullptr),
      storage_(new FilesystemMusicStorage("/"))
{

  ui_->setupUi(this);

  // Icons
  ui_->back->setIcon(IconLoader::Load("go-previous"));
  ui_->forward->setIcon(IconLoader::Load("go-next"));
  ui_->home->setIcon(IconLoader::Load("go-home"));
  ui_->up->setIcon(IconLoader::Load("go-up"));

  connect(ui_->back, SIGNAL(clicked()), undo_stack_, SLOT(undo()));
  connect(ui_->forward, SIGNAL(clicked()), undo_stack_, SLOT(redo()));
  connect(ui_->home, SIGNAL(clicked()), SLOT(FileHome()));
  connect(ui_->up, SIGNAL(clicked()), SLOT(FileUp()));
  connect(ui_->path, SIGNAL(textChanged(QString)), SLOT(ChangeFilePath(QString)));

  connect(undo_stack_, SIGNAL(canUndoChanged(bool)), ui_->back, SLOT(setEnabled(bool)));
  connect(undo_stack_, SIGNAL(canRedoChanged(bool)), ui_->forward, SLOT(setEnabled(bool)));

  connect(ui_->list, SIGNAL(activated(QModelIndex)), SLOT(ItemActivated(QModelIndex)));
  connect(ui_->list, SIGNAL(doubleClicked(QModelIndex)), SLOT(ItemDoubleClick(QModelIndex)));
  connect(ui_->list, SIGNAL(AddToPlaylist(QMimeData*)), SIGNAL(AddToPlaylist(QMimeData*)));
  connect(ui_->list, SIGNAL(CopyToCollection(QList<QUrl>)), SIGNAL(CopyToCollection(QList<QUrl>)));
  connect(ui_->list, SIGNAL(MoveToCollection(QList<QUrl>)), SIGNAL(MoveToCollection(QList<QUrl>)));
  connect(ui_->list, SIGNAL(CopyToDevice(QList<QUrl>)), SIGNAL(CopyToDevice(QList<QUrl>)));
#ifdef HAVE_GSTREAMER
  connect(ui_->list, SIGNAL(Delete(QStringList)), SLOT(Delete(QStringList)));
#endif
  connect(ui_->list, SIGNAL(EditTags(QList<QUrl>)), SIGNAL(EditTags(QList<QUrl>)));

  QString filter(FileView::kFileFilter);
  filter_list_ << filter.split(" ");

  ReloadSettings();

}

FileView::~FileView() {
  delete ui_;
}

void FileView::ReloadSettings() {

  QSettings s;
  s.beginGroup(AppearanceSettingsPage::kSettingsGroup);
  int iconsize = s.value(AppearanceSettingsPage::kIconSizeLeftPanelButtons, 22).toInt();
  s.endGroup();

  ui_->back->setIconSize(QSize(iconsize, iconsize));
  ui_->forward->setIconSize(QSize(iconsize, iconsize));
  ui_->home->setIconSize(QSize(iconsize, iconsize));
  ui_->up->setIconSize(QSize(iconsize, iconsize));

}

void FileView::SetPath(const QString &path) {
  if (!model_)
    lazy_set_path_ = path;
  else
    ChangeFilePathWithoutUndo(path);
}

void FileView::SetTaskManager(TaskManager *task_manager) {
  task_manager_ = task_manager;
}

void FileView::FileUp() {

  QDir dir(model_->rootDirectory());
  dir.cdUp();

  // Is this the same as going back?  If so just go back, so we can keep the view scroll position.
  if (undo_stack_->canUndo()) {
    const UndoCommand *last_dir = static_cast<const UndoCommand*>(undo_stack_->command(undo_stack_->index()-1));
    if (last_dir->undo_path() == dir.path()) {
      undo_stack_->undo();
      return;
    }
  }

  ChangeFilePath(dir.path());

}

void FileView::FileHome() {
  ChangeFilePath(QDir::homePath());
}

void FileView::ChangeFilePath(const QString &new_path_native) {

  QString new_path = QDir::fromNativeSeparators(new_path_native);

  QFileInfo info(new_path);
  if (!info.exists() || !info.isDir())
    return;

  QString old_path(model_->rootPath());
  if (old_path == new_path)
    return;

  undo_stack_->push(new UndoCommand(this, new_path));

}

void FileView::ChangeFilePathWithoutUndo(const QString &new_path) {

  ui_->list->setRootIndex(model_->setRootPath(new_path));
  ui_->path->setText(QDir::toNativeSeparators(new_path));

  QDir dir(new_path);
  ui_->up->setEnabled(dir.cdUp());

  emit PathChanged(new_path);

}

void FileView::ItemActivated(const QModelIndex &idx) {
  if (model_->isDir(idx))
    ChangeFilePath(model_->filePath(idx));
}

void FileView::ItemDoubleClick(const QModelIndex &idx) {

  if (model_->isDir(idx))
    return;

  QString file_path = model_->filePath(idx);

  MimeData *mimedata = new MimeData;
  mimedata->from_doubleclick_ = true;
  mimedata->setUrls(QList<QUrl>() << QUrl::fromLocalFile(file_path));
  mimedata->name_for_new_playlist_ = file_path;

  emit AddToPlaylist(mimedata);

}


FileView::UndoCommand::UndoCommand(FileView *view, const QString &new_path)
  : view_(view) {

  old_state_.path = view->model_->rootPath();
  old_state_.scroll_pos = view_->ui_->list->verticalScrollBar()->value();
  old_state_.index = view_->ui_->list->currentIndex();

  new_state_.path = new_path;
}

void FileView::UndoCommand::redo() {

  view_->ChangeFilePathWithoutUndo(new_state_.path);
  if (new_state_.scroll_pos != -1) {
    view_->ui_->list->setCurrentIndex(new_state_.index);
    view_->ui_->list->verticalScrollBar()->setValue(new_state_.scroll_pos);
  }

}

void FileView::UndoCommand::undo() {

  new_state_.scroll_pos = view_->ui_->list->verticalScrollBar()->value();
  new_state_.index = view_->ui_->list->currentIndex();

  view_->ChangeFilePathWithoutUndo(old_state_.path);
  view_->ui_->list->setCurrentIndex(old_state_.index);
  view_->ui_->list->verticalScrollBar()->setValue(old_state_.scroll_pos);

}

void FileView::Delete(const QStringList &filenames) {

#ifdef HAVE_GSTREAMER

  if (filenames.isEmpty())
    return;

  if (QMessageBox::warning(this, tr("Delete files"),
        tr("These files will be deleted from disk, are you sure you want to continue?"),
        QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
    return;

  DeleteFiles *delete_files = new DeleteFiles(task_manager_, storage_);
  connect(delete_files, SIGNAL(Finished(SongList)), SLOT(DeleteFinished(SongList)));
  delete_files->Start(filenames);

#else
  Q_UNUSED(filenames)
#endif

}

void FileView::DeleteFinished(const SongList &songs_with_errors) {

#ifdef HAVE_GSTREAMER

  if (songs_with_errors.isEmpty()) return;

  OrganiseErrorDialog *dialog = new OrganiseErrorDialog(this);
  dialog->Show(OrganiseErrorDialog::Type_Delete, songs_with_errors);
  // It deletes itself when the user closes it

#else
  Q_UNUSED(songs_with_errors)
#endif

}

void FileView::showEvent(QShowEvent *e) {

  QWidget::showEvent(e);

  if (model_) return;

  model_ = new QFileSystemModel(this);

  model_->setNameFilters(filter_list_);
  // if an item fails the filter, hide it
  model_->setNameFilterDisables(false);

  ui_->list->setModel(model_);
  ChangeFilePathWithoutUndo(QDir::homePath());

  if (!lazy_set_path_.isEmpty()) ChangeFilePathWithoutUndo(lazy_set_path_);

}

void FileView::keyPressEvent(QKeyEvent *e) {

  switch (e->key()) {
    case Qt::Key_Back:
    case Qt::Key_Backspace:
      ui_->up->click();
      break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
      ItemDoubleClick(ui_->list->currentIndex());
      break;
  }

  QWidget::keyPressEvent(e);

}

