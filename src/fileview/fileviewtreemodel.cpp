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

#include <QObject>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QMimeData>
#include <QUrl>
#include <QIcon>

#include "core/simpletreemodel.h"
#include "core/logging.h"
#include "fileviewtreemodel.h"
#include "fileviewtreeitem.h"

using namespace Qt::Literals::StringLiterals;

FileViewTreeModel::FileViewTreeModel(QObject *parent)
    : SimpleTreeModel<FileViewTreeItem>(new FileViewTreeItem(this), parent),
      icon_provider_(new QFileIconProvider()) {
}

FileViewTreeModel::~FileViewTreeModel() {
  delete root_;
  delete icon_provider_;
}

Qt::ItemFlags FileViewTreeModel::flags(const QModelIndex &idx) const {

  const FileViewTreeItem *item = IndexToItem(idx);
  if (!item) return Qt::NoItemFlags;

  switch (item->type) {
    case FileViewTreeItem::Type::VirtualRoot:
    case FileViewTreeItem::Type::Directory:
    case FileViewTreeItem::Type::File:
      return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    case FileViewTreeItem::Type::Root:
    default:
      return Qt::ItemIsEnabled;
  }

}

QVariant FileViewTreeModel::data(const QModelIndex &idx, const int role) const {

  if (!idx.isValid()) return QVariant();

  const FileViewTreeItem *item = IndexToItem(idx);
  if (!item) return QVariant();

  switch (role) {
    case Qt::DisplayRole:
      if (item->type == FileViewTreeItem::Type::VirtualRoot) {
        return item->display_text.isEmpty() ? item->file_path : item->display_text;
      }
      return item->file_info.fileName();

    case Qt::DecorationRole:
      return GetIcon(item);

    case Role_Type:
      return QVariant::fromValue(item->type);

    case Role_FilePath:
      return item->file_path;

    case Role_FileName:
      return item->file_info.fileName();

    default:
      return QVariant();
  }

}

bool FileViewTreeModel::hasChildren(const QModelIndex &parent) const {

  const FileViewTreeItem *item = IndexToItem(parent);
  if (!item) return false;

  // Root and VirtualRoot always have children (or can have them)
  if (item->type == FileViewTreeItem::Type::Root) return true;
  if (item->type == FileViewTreeItem::Type::VirtualRoot) return true;

  // Directories can have children
  if (item->type == FileViewTreeItem::Type::Directory) {
    return true;
  }

  // Files don't have children
  return false;

}

bool FileViewTreeModel::canFetchMore(const QModelIndex &parent) const {

  const FileViewTreeItem *item = IndexToItem(parent);
  if (!item) return false;

  // Can fetch more if not yet lazy loaded
  return !item->lazy_loaded && (item->type == FileViewTreeItem::Type::VirtualRoot || item->type == FileViewTreeItem::Type::Directory);

}

void FileViewTreeModel::fetchMore(const QModelIndex &parent) {

  FileViewTreeItem *item = IndexToItem(parent);
  if (!item || item->lazy_loaded) return;

  LazyLoad(item);

}

void FileViewTreeModel::LazyLoad(FileViewTreeItem *item) {

  if (item->lazy_loaded) return;

  QDir dir(item->file_path);
  if (!dir.exists()) {
    item->lazy_loaded = true;
    return;
  }

  // Apply name filters
  QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot;
  if (!name_filters_.isEmpty()) {
    dir.setNameFilters(name_filters_);
  }

  QFileInfoList entries = dir.entryInfoList(filters, QDir::Name | QDir::DirsFirst);

  if (!entries.isEmpty()) {
    BeginInsert(item, 0, static_cast<int>(entries.count()) - 1);

    for (const QFileInfo &entry : entries) {
      FileViewTreeItem *child = new FileViewTreeItem(
        entry.isDir() ? FileViewTreeItem::Type::Directory : FileViewTreeItem::Type::File,
        item
      );
      child->file_path = entry.absoluteFilePath();
      child->file_info = entry;
      child->lazy_loaded = false;
      child->display_text = entry.fileName();
    }

    EndInsert();
  }

  item->lazy_loaded = true;

}

QIcon FileViewTreeModel::GetIcon(const FileViewTreeItem *item) const {

  if (!item) return QIcon();

  switch (item->type) {
    case FileViewTreeItem::Type::VirtualRoot:
    case FileViewTreeItem::Type::Directory:
      return icon_provider_->icon(QFileIconProvider::Folder);
    case FileViewTreeItem::Type::File:
      return icon_provider_->icon(item->file_info);
    default:
      return QIcon();
  }

}

QStringList FileViewTreeModel::mimeTypes() const {
  return QStringList() << u"text/uri-list"_s;
}

QMimeData *FileViewTreeModel::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  QList<QUrl> urls;
  for (const QModelIndex &idx : indexes) {
    const FileViewTreeItem *item = IndexToItem(idx);
    if (item && (item->type == FileViewTreeItem::Type::File || item->type == FileViewTreeItem::Type::Directory || item->type == FileViewTreeItem::Type::VirtualRoot)) {
      urls << QUrl::fromLocalFile(item->file_path);
    }
  }

  if (urls.isEmpty()) return nullptr;

  QMimeData *data = new QMimeData();
  data->setUrls(urls);
  return data;

}

void FileViewTreeModel::SetRootPaths(const QStringList &paths) {

  Reset();

  for (const QString &path : paths) {
    QFileInfo info(path);
    if (!info.exists() || !info.isDir()) continue;

    FileViewTreeItem *virtual_root = new FileViewTreeItem(FileViewTreeItem::Type::VirtualRoot, root_);
    virtual_root->file_path = info.absoluteFilePath();
    virtual_root->file_info = info;
    virtual_root->display_text = info.absoluteFilePath();
    virtual_root->lazy_loaded = false;
  }

}

void FileViewTreeModel::SetNameFilters(const QStringList &filters) {
  name_filters_ = filters;
}

void FileViewTreeModel::Reset() {

  beginResetModel();

  // Clear children without notifications since we're in a reset
  qDeleteAll(root_->children);
  root_->children.clear();

  endResetModel();

}
