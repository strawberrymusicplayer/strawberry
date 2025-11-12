/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>

#include <QWidget>
#include <QAbstractItemModel>
#include <QFileInfo>
#include <QDir>
#include <QMenu>
#include <QUrl>
#include <QCollator>
#include <QtEvents>

#include "core/iconloader.h"
#include "core/mimedata.h"
#include "utilities/filemanagerutils.h"
#include "fileviewtree.h"
#include "fileviewtreemodel.h"

using namespace Qt::Literals::StringLiterals;

FileViewTree::FileViewTree(QWidget *parent)
    : QTreeView(parent),
      menu_(new QMenu(this)) {

  menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Append to current playlist"), this, &FileViewTree::AddToPlaylistSlot);
  menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Replace current playlist"), this, &FileViewTree::LoadSlot);
  menu_->addAction(IconLoader::Load(u"document-new"_s), tr("Open in new playlist"), this, &FileViewTree::OpenInNewPlaylistSlot);
  menu_->addSeparator();
  menu_->addAction(IconLoader::Load(u"edit-copy"_s), tr("Copy to collection..."), this, &FileViewTree::CopyToCollectionSlot);
  menu_->addAction(IconLoader::Load(u"go-jump"_s), tr("Move to collection..."), this, &FileViewTree::MoveToCollectionSlot);
  menu_->addAction(IconLoader::Load(u"device"_s), tr("Copy to device..."), this, &FileViewTree::CopyToDeviceSlot);
  menu_->addAction(IconLoader::Load(u"edit-delete"_s), tr("Delete from disk..."), this, &FileViewTree::DeleteSlot);

  menu_->addSeparator();
  menu_->addAction(IconLoader::Load(u"edit-rename"_s), tr("Edit track information..."), this, &FileViewTree::EditTagsSlot);
  menu_->addAction(IconLoader::Load(u"document-open-folder"_s), tr("Show in file browser..."), this, &FileViewTree::ShowInBrowser);

  setAttribute(Qt::WA_MacShowFocusRect, false);
  setHeaderHidden(true);
  setUniformRowHeights(true);

}

void FileViewTree::contextMenuEvent(QContextMenuEvent *e) {

  menu_selection_ = selectionModel()->selection();

  menu_->popup(e->globalPos());
  e->accept();

}

QStringList FileViewTree::FilenamesFromSelection() const {

  QStringList filenames;
  const QModelIndexList indexes = menu_selection_.indexes();

  FileViewTreeModel *tree_model = qobject_cast<FileViewTreeModel*>(model());
  if (tree_model) {
    for (const QModelIndex &index : indexes) {
      if (index.column() == 0) {
        QString path = tree_model->data(index, FileViewTreeModel::Role_FilePath).toString();
        if (!path.isEmpty()) {
          filenames << path;
        }
      }
    }
  }

  QCollator collator;
  collator.setNumericMode(true);
  std::sort(filenames.begin(), filenames.end(), collator);

  return filenames;

}

QList<QUrl> FileViewTree::UrlListFromSelection() const {

  QList<QUrl> urls;
  const QStringList filenames = FilenamesFromSelection();
  urls.reserve(filenames.count());
  for (const QString &filename : std::as_const(filenames)) {
    urls << QUrl::fromLocalFile(filename);
  }

  return urls;

}

MimeData *FileViewTree::MimeDataFromSelection() const {

  MimeData *mimedata = new MimeData;
  mimedata->setUrls(UrlListFromSelection());

  const QStringList filenames = FilenamesFromSelection();

  // if just one folder selected - use its path as the new playlist's name
  if (filenames.size() == 1 && QFileInfo(filenames.first()).isDir()) {
    if (filenames.first().length() > 20) {
      mimedata->name_for_new_playlist_ = QDir(filenames.first()).dirName();
    }
    else {
      mimedata->name_for_new_playlist_ = filenames.first();
    }
  }
  // otherwise, use "Files" as default
  else {
    mimedata->name_for_new_playlist_ = tr("Files");
  }

  return mimedata;

}

void FileViewTree::LoadSlot() {

  MimeData *mimedata = MimeDataFromSelection();
  mimedata->clear_first_ = true;
  Q_EMIT AddToPlaylist(mimedata);

}

void FileViewTree::AddToPlaylistSlot() {
  Q_EMIT AddToPlaylist(MimeDataFromSelection());
}

void FileViewTree::OpenInNewPlaylistSlot() {

  MimeData *mimedata = MimeDataFromSelection();
  mimedata->open_in_new_playlist_ = true;
  Q_EMIT AddToPlaylist(mimedata);

}

void FileViewTree::CopyToCollectionSlot() {
  Q_EMIT CopyToCollection(UrlListFromSelection());
}

void FileViewTree::MoveToCollectionSlot() {
  Q_EMIT MoveToCollection(UrlListFromSelection());
}

void FileViewTree::CopyToDeviceSlot() {
  Q_EMIT CopyToDevice(UrlListFromSelection());
}

void FileViewTree::DeleteSlot() {
  Q_EMIT Delete(FilenamesFromSelection());
}

void FileViewTree::EditTagsSlot() {
  Q_EMIT EditTags(UrlListFromSelection());
}

void FileViewTree::mousePressEvent(QMouseEvent *e) {

  switch (e->button()) {
    // Enqueue to playlist with middleClick
    case Qt::MiddleButton:{
      QTreeView::mousePressEvent(e);

      // We need to update the menu selection
      QItemSelectionModel *selection_model = selectionModel();
      if (!selection_model) {
        e->ignore();
        return;
      }
      menu_selection_ = selection_model->selection();

      MimeData *mimedata = new MimeData;
      mimedata->setUrls(UrlListFromSelection());
      mimedata->enqueue_now_ = true;
      Q_EMIT AddToPlaylist(mimedata);
      break;
    }
    default:
      QTreeView::mousePressEvent(e);
      break;
  }

}

void FileViewTree::ShowInBrowser() {
  Utilities::OpenInFileBrowser(UrlListFromSelection());
}
