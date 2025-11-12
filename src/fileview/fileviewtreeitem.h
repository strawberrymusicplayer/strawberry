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

#ifndef FILEVIEWTREEITEM_H
#define FILEVIEWTREEITEM_H

#include "config.h"

#include <QFileInfo>

#include "core/simpletreeitem.h"

class FileViewTreeItem : public SimpleTreeItem<FileViewTreeItem> {
 public:
  enum class Type {
    Root,          // Hidden root
    VirtualRoot,   // User-configured root paths
    Directory,     // File system directory
    File           // File system file
  };

  explicit FileViewTreeItem(SimpleTreeModel<FileViewTreeItem> *_model) : SimpleTreeItem<FileViewTreeItem>(_model), type(Type::Root), lazy_loaded(false) {}
  explicit FileViewTreeItem(const Type _type, FileViewTreeItem *_parent = nullptr) : SimpleTreeItem<FileViewTreeItem>(_parent), type(_type), lazy_loaded(false) {}

  Type type;
  QString file_path;        // Absolute file system path
  QFileInfo file_info;      // Cached file info
  bool lazy_loaded;         // Whether children have been loaded

 private:
  Q_DISABLE_COPY(FileViewTreeItem)
};

Q_DECLARE_METATYPE(FileViewTreeItem::Type)

#endif  // FILEVIEWTREEITEM_H
