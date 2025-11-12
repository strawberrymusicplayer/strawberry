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
#include <QStandardPaths>
#include <QMessageBox>
#include <QScrollBar>
#include <QLineEdit>
#include <QToolButton>
#include <QFileDialog>
#include <QSpacerItem>
#include <QtEvents>

#include "constants/appearancesettings.h"
#include "constants/filefilterconstants.h"
#include "includes/shared_ptr.h"
#include "core/deletefiles.h"
#include "core/filesystemmusicstorage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "core/mimedata.h"
#include "dialogs/deleteconfirmationdialog.h"
#include "fileview.h"
#include "fileviewlist.h"
#include "fileviewtree.h"
#include "fileviewtreemodel.h"
#include "fileviewtreeitem.h"
#include "ui_fileview.h"
#include "organize/organizeerrordialog.h"

using std::make_unique;
using namespace Qt::Literals::StringLiterals;

FileView::FileView(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_FileView),
      model_(nullptr),
      tree_model_(nullptr),
      undo_stack_(new QUndoStack(this)),
      task_manager_(nullptr),
      storage_(new FilesystemMusicStorage(Song::Source::LocalFile, u"/"_s)),
      tree_view_active_(false),
      view_mode_spacer_(nullptr) {

  ui_->setupUi(this);

  // Icons
  ui_->back->setIcon(IconLoader::Load(u"go-previous"_s));
  ui_->forward->setIcon(IconLoader::Load(u"go-next"_s));
  ui_->home->setIcon(IconLoader::Load(u"go-home"_s));
  ui_->up->setIcon(IconLoader::Load(u"go-up"_s));
  ui_->toggle_view->setIcon(IconLoader::Load(u"view-choose"_s));

  QObject::connect(ui_->back, &QToolButton::clicked, undo_stack_, &QUndoStack::undo);
  QObject::connect(ui_->forward, &QToolButton::clicked, undo_stack_, &QUndoStack::redo);
  QObject::connect(ui_->home, &QToolButton::clicked, this, &FileView::FileHome);
  QObject::connect(ui_->up, &QToolButton::clicked, this, &FileView::FileUp);
  QObject::connect(ui_->path, &QLineEdit::textChanged, this, &FileView::ChangeFilePath);
  QObject::connect(ui_->toggle_view, &QToolButton::clicked, this, &FileView::ToggleViewMode);

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

  // Connect tree view signals
  QObject::connect(ui_->tree, &FileViewTree::AddToPlaylist, this, &FileView::AddToPlaylist);
  QObject::connect(ui_->tree, &FileViewTree::CopyToCollection, this, &FileView::CopyToCollection);
  QObject::connect(ui_->tree, &FileViewTree::MoveToCollection, this, &FileView::MoveToCollection);
  QObject::connect(ui_->tree, &FileViewTree::CopyToDevice, this, &FileView::CopyToDevice);
  QObject::connect(ui_->tree, &FileViewTree::Delete, this, &FileView::Delete);
  QObject::connect(ui_->tree, &FileViewTree::EditTags, this, &FileView::EditTags);
  QObject::connect(ui_->tree, &FileViewTree::activated, this, &FileView::ItemActivated);
  QObject::connect(ui_->tree, &FileViewTree::doubleClicked, this, &FileView::ItemDoubleClick);

  // Setup tree root management buttons
  ui_->add_tree_root->setIcon(IconLoader::Load(u"folder-new"_s));
  ui_->remove_tree_root->setIcon(IconLoader::Load(u"list-remove"_s));
  QObject::connect(ui_->add_tree_root, &QToolButton::clicked, this, &FileView::AddRootButtonClicked);
  QObject::connect(ui_->remove_tree_root, &QToolButton::clicked, this, &FileView::RemoveRootButtonClicked);

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
  ui_->toggle_view->setIconSize(QSize(iconsize, iconsize));
  ui_->add_tree_root->setIconSize(QSize(iconsize, iconsize));
  ui_->remove_tree_root->setIconSize(QSize(iconsize, iconsize));

  // Load tree root paths setting
  Settings file_settings;
  file_settings.beginGroup(u"FileView"_s);
  tree_root_paths_ = file_settings.value(u"tree_root_paths"_s, QStandardPaths::standardLocations(QStandardPaths::StandardLocation::MusicLocation)).toStringList();
  tree_view_active_ = file_settings.value(u"tree_view_active"_s, false).toBool();
  file_settings.endGroup();

  // Set initial view mode
  UpdateViewModeUI();

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
  // Only handle activation for list view (not tree view)
  if (!tree_view_active_ && model_->isDir(idx)) {
    ChangeFilePath(model_->filePath(idx));
  }
}

void FileView::ItemDoubleClick(const QModelIndex &idx) {

  QString file_path;
  bool is_file = false;

  // Handle tree view with virtual roots
  if (tree_view_active_ && tree_model_) {
    QVariant type_var = tree_model_->data(idx, FileViewTreeModel::Role_Type);
    if (type_var.isValid()) {
      FileViewTreeItem::Type item_type = type_var.value<FileViewTreeItem::Type>();
      // Only handle files, ignore directories and virtual roots
      if (item_type == FileViewTreeItem::Type::File) {
        file_path = tree_model_->data(idx, FileViewTreeModel::Role_FilePath).toString();
        is_file = true;
      }
    }
  }
  // Handle list view with filesystem model
  else if (!tree_view_active_ && model_) {
    if (!model_->isDir(idx)) {
      file_path = model_->filePath(idx);
      is_file = true;
    }
  }

  // Add file to playlist if it's a valid file
  if (is_file && !file_path.isEmpty()) {
    MimeData *mimedata = new MimeData;
    mimedata->from_doubleclick_ = true;
    mimedata->setUrls(QList<QUrl>() << QUrl::fromLocalFile(file_path));
    mimedata->name_for_new_playlist_ = file_path;

    Q_EMIT AddToPlaylist(mimedata);
  }

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

  // Create tree model
  tree_model_ = new FileViewTreeModel(this);
  tree_model_->SetNameFilters(filter_list_);

  SetupTreeView();

  ChangeFilePathWithoutUndo(QDir::homePath());

  if (!lazy_set_path_.isEmpty()) ChangeFilePathWithoutUndo(lazy_set_path_);

}

void FileView::SetupTreeView() {

  // Use the new tree model with virtual roots
  ui_->tree->setModel(tree_model_);

  // Set the root paths in the model
  tree_model_->SetRootPaths(tree_root_paths_);

  // No need to set root index - the model handles virtual roots

}

void FileView::ToggleViewMode() {

  tree_view_active_ = !tree_view_active_;
  UpdateViewModeUI();

  // Save the preference
  Settings s;
  s.beginGroup(u"FileView"_s);
  s.setValue(u"tree_view_active"_s, tree_view_active_);
  s.endGroup();

}

void FileView::UpdateViewModeUI() {

  if (tree_view_active_) {
    ui_->view_stack->setCurrentWidget(ui_->tree_page);
    // Hide navigation controls in tree view mode
    ui_->back->setVisible(false);
    ui_->forward->setVisible(false);
    ui_->up->setVisible(false);
    ui_->home->setVisible(false);
    ui_->path->setVisible(false);
    // Show tree root management buttons
    ui_->add_tree_root->setVisible(true);
    ui_->remove_tree_root->setVisible(true);
    // Insert spacer in tree view if not already present
    if (!view_mode_spacer_) {
      view_mode_spacer_ = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
      ui_->horizontalLayout->insertSpacerItem(ui_->horizontalLayout->indexOf(ui_->toggle_view), view_mode_spacer_);
    }
  }
  else {
    ui_->view_stack->setCurrentWidget(ui_->list_page);
    // Show navigation controls in list view mode
    ui_->back->setVisible(true);
    ui_->forward->setVisible(true);
    ui_->up->setVisible(true);
    ui_->home->setVisible(true);
    ui_->path->setVisible(true);
    // Hide tree root management buttons in list view
    ui_->add_tree_root->setVisible(false);
    ui_->remove_tree_root->setVisible(false);
    // Remove spacer in list view
    if (view_mode_spacer_) {
      ui_->horizontalLayout->removeItem(view_mode_spacer_);
      delete view_mode_spacer_;
      view_mode_spacer_ = nullptr;
    }
  }

}

void FileView::AddTreeRootPath(const QString &path) {

  if (!tree_root_paths_.contains(path)) {
    tree_root_paths_.append(path);
    SaveTreeRootPaths();

    // Refresh the tree view to show the new root
    if (tree_model_) {
      SetupTreeView();
    }
  }

}

void FileView::RemoveTreeRootPath(const QString &path) {

  tree_root_paths_.removeAll(path);
  SaveTreeRootPaths();

  // Refresh the tree view
  if (tree_model_) {
    SetupTreeView();
  }

}

void FileView::SaveTreeRootPaths() {

  Settings s;
  s.beginGroup(u"FileView"_s);
  s.setValue(u"tree_root_paths"_s, tree_root_paths_);
  s.endGroup();

}

void FileView::AddRootButtonClicked() {

  const QString dir = QFileDialog::getExistingDirectory(this, tr("Select folder to add as tree root"), tree_root_paths_.isEmpty() ? QDir::homePath() : tree_root_paths_.first(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (!dir.isEmpty()) {
    AddTreeRootPath(dir);
  }

}

void FileView::RemoveRootButtonClicked() {

  // Get currently selected item in tree
  QModelIndex current = ui_->tree->currentIndex();
  if (!current.isValid()) return;

  QString path;

  // Get the file path from the appropriate model
  if (tree_model_) {
    path = tree_model_->data(current, FileViewTreeModel::Role_FilePath).toString();
  }

  if (path.isEmpty()) return;

  const QString clean_path = QDir::cleanPath(path);

  // Check if this path or any parent is a configured root
  for (const QString &root : std::as_const(tree_root_paths_)) {
    const QString clean_root = QDir::cleanPath(root);
    if (clean_path == clean_root || clean_path.startsWith(clean_root + QDir::separator())) {
      RemoveTreeRootPath(root);
      return;
    }
  }

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
