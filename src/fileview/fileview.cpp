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

#include <memory>

#include <QWidget>
#include <QUndoStack>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileIconProvider>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSettings>
#include <QMessageBox>
#include <QScrollBar>
#include <QLineEdit>
#include <QToolButton>
#include <QtEvents>

#include "includes/shared_ptr.h"
#include "core/deletefiles.h"
#include "core/filesystemmusicstorage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "core/mimedata.h"
#include "dialogs/deleteconfirmationdialog.h"
#include "fileview.h"
#include "fileviewlist.h"
#include "ui_fileview.h"
#include "organize/organizeerrordialog.h"
#include "constants/appearancesettings.h"
#include "constants/filefilterconstants.h"

using std::make_unique;
using namespace Qt::Literals::StringLiterals;

FileView::FileView(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_FileView),
      model_(nullptr),
      undo_stack_(new QUndoStack(this)),
      task_manager_(nullptr),
      storage_(new FilesystemMusicStorage(Song::Source::LocalFile, u"/"_s)) {

  ui_->setupUi(this);

  // Icons
  ui_->back->setIcon(IconLoader::Load(u"go-previous"_s));
  ui_->forward->setIcon(IconLoader::Load(u"go-next"_s));
  ui_->home->setIcon(IconLoader::Load(u"go-home"_s));
  ui_->up->setIcon(IconLoader::Load(u"go-up"_s));

  QObject::connect(ui_->back, &QToolButton::clicked, undo_stack_, &QUndoStack::undo);
  QObject::connect(ui_->forward, &QToolButton::clicked, undo_stack_, &QUndoStack::redo);
  QObject::connect(ui_->home, &QToolButton::clicked, this, &FileView::FileHome);
  QObject::connect(ui_->up, &QToolButton::clicked, this, &FileView::FileUp);
  QObject::connect(ui_->path, &QLineEdit::textChanged, this, &FileView::ChangeFilePath);

  QObject::connect(undo_stack_, &QUndoStack::canUndoChanged, ui_->back, &FileView::setEnabled);
  QObject::connect(undo_stack_, &QUndoStack::canRedoChanged, ui_->forward, &FileView::setEnabled);

  QObject::connect(ui_->list, &FileViewList::activated, this, &FileView::ItemActivated);
  QObject::connect(ui_->list, &FileViewList::doubleClicked, this, &FileView::ItemDoubleClick);
  QObject::connect(ui_->list, &FileViewList::AddToPlaylist, this, &FileView::AddToPlaylist);
  QObject::connect(ui_->list, &FileViewList::CopyToCollection, this, &FileView::CopyToCollection);
  QObject::connect(ui_->list, &FileViewList::MoveToCollection, this, &FileView::MoveToCollection);
  QObject::connect(ui_->list, &FileViewList::CopyToDevice, this, &FileView::CopyToDevice);
  QObject::connect(ui_->list, &FileViewList::Delete, this, &FileView::Delete);
  QObject::connect(ui_->list, &FileViewList::EditTags, this, &FileView::EditTags);

  QString filter = QLatin1String(kFileFilter);
  filter_list_ << filter.split(u' ');

  ReloadSettings();

}

FileView::~FileView() {
  delete ui_;
}

void FileView::ReloadSettings() {

  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  int iconsize = s.value(AppearanceSettings::kIconSizeLeftPanelButtons, 22).toInt();
  s.endGroup();

  ui_->back->setIconSize(QSize(iconsize, iconsize));
  ui_->forward->setIconSize(QSize(iconsize, iconsize));
  ui_->home->setIconSize(QSize(iconsize, iconsize));
  ui_->up->setIconSize(QSize(iconsize, iconsize));

}

void FileView::SetPath(const QString &path) {

  if (model_) {
    ChangeFilePathWithoutUndo(path);
  }
  else {
    lazy_set_path_ = path;
  }

}

void FileView::SetTaskManager(SharedPtr<TaskManager> task_manager) {
  task_manager_ = task_manager;
}

void FileView::FileUp() {

  QDir dir(model_->rootDirectory());
  dir.cdUp();

  // Is this the same as going back?  If so just go back, so we can keep the view scroll position.
  if (undo_stack_->canUndo()) {
    const UndoCommand *last_dir = static_cast<const UndoCommand*>(undo_stack_->command(undo_stack_->index() - 1));
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
  if (!info.exists() || !info.isDir()) {
    return;
  }

  QString old_path(model_->rootPath());
  if (old_path == new_path) {
    return;
  }

  undo_stack_->push(new UndoCommand(this, new_path));

}

void FileView::ChangeFilePathWithoutUndo(const QString &new_path) {

  ui_->list->setRootIndex(model_->setRootPath(new_path));
  ui_->path->setText(QDir::toNativeSeparators(new_path));

  QDir dir(new_path);
  ui_->up->setEnabled(dir.cdUp());

  Q_EMIT PathChanged(new_path);

}

void FileView::ItemActivated(const QModelIndex &idx) {
  if (model_->isDir(idx))
    ChangeFilePath(model_->filePath(idx));
}

void FileView::ItemDoubleClick(const QModelIndex &idx) {

  if (model_->isDir(idx)) {
    return;
  }

  QString file_path = model_->filePath(idx);

  MimeData *mimedata = new MimeData;
  mimedata->from_doubleclick_ = true;
  mimedata->setUrls(QList<QUrl>() << QUrl::fromLocalFile(file_path));
  mimedata->name_for_new_playlist_ = file_path;

  Q_EMIT AddToPlaylist(mimedata);

}


FileView::UndoCommand::UndoCommand(FileView *view, const QString &new_path) : view_(view) {

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

  if (filenames.isEmpty()) return;

  if (DeleteConfirmationDialog::warning(filenames) != QDialogButtonBox::Yes) return;

  DeleteFiles *delete_files = new DeleteFiles(task_manager_, storage_, true);
  QObject::connect(delete_files, &DeleteFiles::Finished, this, &FileView::DeleteFinished);
  delete_files->Start(filenames);

}

void FileView::DeleteFinished(const SongList &songs_with_errors) {

  if (songs_with_errors.isEmpty()) return;

  OrganizeErrorDialog *dialog = new OrganizeErrorDialog(this);
  dialog->Show(OrganizeErrorDialog::OperationType::Delete, songs_with_errors);
  // It deletes itself when the user closes it

}

void FileView::showEvent(QShowEvent *e) {

  QWidget::showEvent(e);

  if (model_) return;

  model_ = new QFileSystemModel(this);
  if (!model_->iconProvider() || model_->iconProvider()->icon(QFileIconProvider::Folder).isNull()) {
    file_icon_provider_ = make_unique<QFileIconProvider>();
    model_->setIconProvider(&*file_icon_provider_);
  }

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
    default:
      break;
  }

  QWidget::keyPressEvent(e);

}
