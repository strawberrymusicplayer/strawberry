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

#ifndef PROJECTMPRESETMODEL_H
#define PROJECTMPRESETMODEL_H

#include "config.h"

#include <QList>
#include <QString>
#include <QAbstractItemModel>

class ProjectMVisualization;

class ProjectMPresetModel : public QAbstractItemModel {
  Q_OBJECT

  friend class ProjectMVisualization;

 public:
  explicit ProjectMPresetModel(ProjectMVisualization *projectm_visualization, QObject *parent = nullptr);

  enum Role {
    Role_Path = Qt::UserRole,
  };

  void MarkSelected(const QString &path, bool selected);

  // QAbstractItemModel
  QModelIndex index(const int row, const int column, const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &child) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, const int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  bool setData(const QModelIndex &index, const QVariant &value, const int role = Qt::EditRole) override;

 public slots:
  void SetImmediatePreset(const QModelIndex &index);
  void SelectAll();
  void SelectNone();

 private:
  struct Preset {
    explicit Preset(const QString &path, const QString &name, const bool selected)
        : path_(path),
          name_(name),
          selected_(selected) {}

    QString path_;
    QString name_;
    bool selected_;
  };

  ProjectMVisualization *projectm_visualization_;
  QList<Preset> all_presets_;
};

#endif  // PROJECTMPRESETMODEL_H
