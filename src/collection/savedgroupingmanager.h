/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2015, Nick Lanham <nick@afternight.org>
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

#ifndef SAVEDGROUPINGMANAGER_H
#define SAVEDGROUPINGMANAGER_H

#include "config.h"

#include <QDialog>
#include <QObject>
#include <QString>

#include "collectionmodel.h"

class QWidget;
class QStandardItemModel;

class CollectionFilterWidget;
class Ui_SavedGroupingManager;

class SavedGroupingManager : public QDialog {
  Q_OBJECT

 public:
  explicit SavedGroupingManager(QWidget *parent = nullptr);
  ~SavedGroupingManager();

  void UpdateModel();
  void SetFilter(CollectionFilterWidget* filter) { filter_ = filter; }

  static QString GroupByToString(const CollectionModel::GroupBy &g);

 private slots:
  void UpdateButtonState();
  void Remove();

 private:
  Ui_SavedGroupingManager* ui_;
  QStandardItemModel *model_;
  CollectionFilterWidget *filter_;
};

#endif  // SAVEDGROUPINGMANAGER_H
