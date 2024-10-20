/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QStandardItemModel>
#include <QVariant>
#include <QString>

#include "core/iconloader.h"
#include "collectionsettingsdirectorymodel.h"

using namespace Qt::Literals::StringLiterals;

CollectionSettingsDirectoryModel::CollectionSettingsDirectoryModel(QObject *parent)
    : QStandardItemModel(parent),
      dir_icon_(IconLoader::Load(u"document-open-folder"_s)) {}

void CollectionSettingsDirectoryModel::AddDirectory(const QString &path) {

  QStandardItem *item = new QStandardItem(path);
  item->setIcon(dir_icon_);
  appendRow(item);

  paths_ << path;

}

void CollectionSettingsDirectoryModel::AddDirectories(const QStringList &paths) {

  for (const QString &path : paths) {
    AddDirectory(path);
  }

}

void CollectionSettingsDirectoryModel::RemoveDirectory(const QModelIndex &idx) {

  if (!idx.isValid()) return;

  const QString path = data(idx).toString();

  removeRow(idx.row());

  if (paths_.contains(path)) {
    paths_.removeAll(path);
  }

}
