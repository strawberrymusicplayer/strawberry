/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

class QModelIndex;
class SettingsDialog;
class Ui_CollectionSettingsPage;

class CollectionSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit CollectionSettingsPage(SettingsDialog *dialog);
  ~CollectionSettingsPage();

  static const char *kSettingsGroup;
  static const char *kSettingsCacheSize;
  static const char *kSettingsCacheSizeUnit;
  static const char *kSettingsDiskCacheEnable;
  static const char *kSettingsDiskCacheSize;
  static const char *kSettingsDiskCacheSizeUnit;

  enum CacheSizeUnit {
    CacheSizeUnit_KB,
    CacheSizeUnit_MB,
    CacheSizeUnit_GB,
    CacheSizeUnit_TB,
  };

  enum SaveCover {
    SaveCover_Hash = 1,
    SaveCover_Pattern = 2
  };

  void Load();
  void Save();

 private slots:
  void Add();
  void Remove();

  void CurrentRowChanged(const QModelIndex &index);
  void DiskCacheEnable(int state);
  void CoverSaveInAlbumDirChanged();

 private:
  Ui_CollectionSettingsPage *ui_;
  bool initialised_model_;

  static const QStringList cacheUnitNames;
};

#endif  // COLLECTIONSETTINGSPAGE_H
