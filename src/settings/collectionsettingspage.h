/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONSETTINGSPAGE_H
#define COLLECTIONSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include "settingspage.h"

#include "includes/shared_ptr.h"

class QModelIndex;
class SettingsDialog;
class CollectionLibrary;
class CollectionBackend;
class CollectionModel;
class CollectionDirectoryModel;
class CollectionSettingsDirectoryModel;
class Ui_CollectionSettingsPage;

class CollectionSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit CollectionSettingsPage(SettingsDialog *dialog, const SharedPtr<CollectionLibrary> collection, const SharedPtr<CollectionBackend> collection_backend, CollectionModel *collection_model, CollectionDirectoryModel *collection_directory_model, QWidget *parent = nullptr);
  ~CollectionSettingsPage() override;

  void Load() override;
  void Save() override;

 private Q_SLOTS:
  void AddDirectory();
  void RemoveDirectory();

  void CurrentRowChanged(const QModelIndex &idx);
  void SongTrackingToggled();
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  void DiskCacheEnable(const Qt::CheckState state);
#else
  void DiskCacheEnable(const int state);
#endif
  void ClearPixmapDiskCache();
  void CacheSizeUnitChanged(int index);
  void DiskCacheSizeUnitChanged(int index);
  void WriteAllSongsStatisticsToFiles();

 private:
  void UpdateIconDiskCacheSize();

 private:
  Ui_CollectionSettingsPage *ui_;

  const SharedPtr<CollectionLibrary> collection_;
  const SharedPtr<CollectionBackend> collection_backend_;
  CollectionModel *collection_model_;
  CollectionSettingsDirectoryModel *collectionsettings_directory_model_;
  CollectionDirectoryModel *collection_directory_model_;
  bool initialized_model_;
};

#endif  // COLLECTIONSETTINGSPAGE_H
