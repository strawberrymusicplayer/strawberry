/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2015, Nick Lanham <nick@afternight.org>
 * Copyright 2019-2022, Jonas Kvinge <jonas@jkvinge.net>
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
  explicit SavedGroupingManager(const QString &saved_groupings_settings_group, QWidget *parent = nullptr);
  ~SavedGroupingManager() override;

  static const char *kSavedGroupingsSettingsGroup;

  static QString GetSavedGroupingsSettingsGroup(const QString &settings_group);

  void UpdateModel();

  static QString GroupByToString(const CollectionModel::GroupBy g);

 Q_SIGNALS:
  void UpdateGroupByActions();

 private Q_SLOTS:
  void UpdateButtonState();
  void Remove();

 private:
  Ui_SavedGroupingManager *ui_;
  QStandardItemModel *model_;
  QString saved_groupings_settings_group_;
};

#endif  // SAVEDGROUPINGMANAGER_H
