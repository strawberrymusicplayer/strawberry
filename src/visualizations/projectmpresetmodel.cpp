/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QAbstractItemModel>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

#include "core/logging.h"

#include "projectmpresetmodel.h"
#include "projectmvisualization.h"

ProjectMPresetModel::ProjectMPresetModel(ProjectMVisualization *projectm_visualization, QObject *parent)
    : QAbstractItemModel(parent),
      projectm_visualization_(projectm_visualization) {

  // Find presets
  if (QFileInfo::exists(projectm_visualization_->preset_path())) {
    QDirIterator it(projectm_visualization_->preset_path(), QStringList() << QStringLiteral("*.milk") << QStringLiteral("*.prjm"), QDir::Files | QDir::NoDotAndDotDot | QDir::Readable, QDirIterator::Subdirectories);
    QStringList files;
    while (it.hasNext()) {
      it.next();
      files << it.filePath();
    }
    std::stable_sort(files.begin(), files.end());
    for (const QString &filepath : std::as_const(files)) {
      const QFileInfo fileinfo(filepath);
      all_presets_ << Preset(fileinfo.filePath(), fileinfo.fileName(), false);
    }
  }
  else {
    qLog(Error) << "ProjectM preset path" << projectm_visualization_->preset_path() << "does not exist";
  }

}

int ProjectMPresetModel::rowCount(const QModelIndex &idx) const {

  Q_UNUSED(idx);

  if (!projectm_visualization_) return 0;

  return static_cast<int>(all_presets_.count());

}

int ProjectMPresetModel::columnCount(const QModelIndex &idx) const {
  Q_UNUSED(idx);
  return 1;
}

QModelIndex ProjectMPresetModel::index(const int row, const int column, const QModelIndex &idx) const {
  Q_UNUSED(idx);
  return createIndex(row, column);
}

QModelIndex ProjectMPresetModel::parent(const QModelIndex &child) const {
  Q_UNUSED(child);
  return QModelIndex();
}

QVariant ProjectMPresetModel::data(const QModelIndex &index, const int role) const {

  switch (role) {
    case Qt::DisplayRole:
      return all_presets_[index.row()].name_;
    case Qt::CheckStateRole:{
      bool selected = all_presets_[index.row()].selected_;
      return selected ? Qt::Checked : Qt::Unchecked;
    }
    case Role::Role_Path:
      return all_presets_[index.row()].path_;
    default:
      return QVariant();
  }

}

Qt::ItemFlags ProjectMPresetModel::flags(const QModelIndex &idx) const {

  if (!idx.isValid()) return QAbstractItemModel::flags(idx);
  return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;

}

bool ProjectMPresetModel::setData(const QModelIndex &idx, const QVariant &value, int role) {

  if (role == Qt::CheckStateRole) {
    all_presets_[idx.row()].selected_ = value.toBool();
    projectm_visualization_->SetSelected(QStringList() << all_presets_[idx.row()].path_, value.toBool());
    return true;
  }

  return false;

}

void ProjectMPresetModel::SetImmediatePreset(const QModelIndex &idx) {
  projectm_visualization_->SetImmediatePreset(all_presets_[idx.row()].path_);
}

void ProjectMPresetModel::SelectAll() {

  QStringList paths;
  paths.reserve(all_presets_.count());
  for (int i = 0; i < all_presets_.count(); ++i) {
    paths << all_presets_[i].path_;
    all_presets_[i].selected_ = true;
  }
  projectm_visualization_->SetSelected(paths, true);

  emit dataChanged(index(0, 0), index(rowCount() - 1, 0));

}

void ProjectMPresetModel::SelectNone() {

  projectm_visualization_->ClearSelected();
  for (int i = 0; i < all_presets_.count(); ++i) {
    all_presets_[i].selected_ = false;
  }

  emit dataChanged(index(0, 0), index(rowCount() - 1, 0));

}

void ProjectMPresetModel::MarkSelected(const QString &path, const bool selected) {

  for (int i = 0; i < all_presets_.count(); ++i) {
    if (path == all_presets_[i].path_) {
      all_presets_[i].selected_ = selected;
      return;
    }
  }

}
