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

#include "config.h"

#include <QStandardPaths>
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QFileDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QSettings>

#include "core/application.h"
#include "core/iconloader.h"
#include "core/utilities.h"
#include "collection/collectionmodel.h"
#include "collection/collectiondirectorymodel.h"
#include "collectionsettingspage.h"
#include "playlist/playlistdelegates.h"
#include "settings/settingsdialog.h"
#include "settings/settingspage.h"
#include "ui_collectionsettingspage.h"

const char *CollectionSettingsPage::kSettingsGroup = "Collection";
const char *CollectionSettingsPage::kSettingsCacheSize = "cache_size";
const char *CollectionSettingsPage::kSettingsCacheSizeUnit = "cache_size_unit";
const char *CollectionSettingsPage::kSettingsDiskCacheEnable = "disk_cache_enable";
const char *CollectionSettingsPage::kSettingsDiskCacheSize = "disk_cache_size";
const char *CollectionSettingsPage::kSettingsDiskCacheSizeUnit = "disk_cache_size_unit";
const int CollectionSettingsPage::kSettingsCacheSizeDefault = 160;
const int CollectionSettingsPage::kSettingsDiskCacheSizeDefault = 360;

const QStringList CollectionSettingsPage::cacheUnitNames = { "KB", "MB", "GB", "TB" };

CollectionSettingsPage::CollectionSettingsPage(SettingsDialog *dialog)
    : SettingsPage(dialog),
      ui_(new Ui_CollectionSettingsPage),
      initialized_model_(false)
      {

  ui_->setupUi(this);
  ui_->list->setItemDelegate(new NativeSeparatorsDelegate(this));

  // Icons
  setWindowIcon(IconLoader::Load("library-music"));
  ui_->add->setIcon(IconLoader::Load("document-open-folder"));

  ui_->combobox_cache_size->addItems(cacheUnitNames);
  ui_->combobox_disk_cache_size->addItems(cacheUnitNames);

  connect(ui_->add, SIGNAL(clicked()), SLOT(Add()));
  connect(ui_->remove, SIGNAL(clicked()), SLOT(Remove()));

  connect(ui_->checkbox_cover_album_dir, SIGNAL(toggled(bool)), SLOT(CoverSaveInAlbumDirChanged()));
  connect(ui_->radiobutton_cover_hash, SIGNAL(toggled(bool)), SLOT(CoverSaveInAlbumDirChanged()));
  connect(ui_->radiobutton_cover_pattern, SIGNAL(toggled(bool)), SLOT(CoverSaveInAlbumDirChanged()));

  connect(ui_->checkbox_disk_cache, SIGNAL(stateChanged(int)), SLOT(DiskCacheEnable(int)));
  connect(ui_->button_clear_disk_cache, SIGNAL(clicked()), dialog->app(), SIGNAL(ClearPixmapDiskCache()));
  connect(ui_->button_clear_disk_cache, SIGNAL(clicked()), SLOT(ClearPixmapDiskCache()));

}

CollectionSettingsPage::~CollectionSettingsPage() { delete ui_; }

void CollectionSettingsPage::Add() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  QString path(s.value("last_path", QStandardPaths::writableLocation(QStandardPaths::MusicLocation)).toString());
  path = QFileDialog::getExistingDirectory(this, tr("Add directory..."), path);

  if (!path.isNull()) {
    dialog()->collection_directory_model()->AddDirectory(path);
  }

  s.setValue("last_path", path);

  set_changed();

}

void CollectionSettingsPage::Remove() {

  dialog()->collection_directory_model()->RemoveDirectory(ui_->list->currentIndex());
  set_changed();

}

void CollectionSettingsPage::CurrentRowChanged(const QModelIndex& index) {
  ui_->remove->setEnabled(index.isValid());
}

void CollectionSettingsPage::DiskCacheEnable(const int state) {

  bool checked = state == Qt::Checked;
  ui_->label_disk_cache_size->setEnabled(checked);
  ui_->spinbox_disk_cache_size->setEnabled(checked);
  ui_->combobox_disk_cache_size->setEnabled(checked);
  ui_->label_disk_cache_in_use->setEnabled(checked);
  ui_->disk_cache_in_use->setEnabled(checked);
  ui_->button_clear_disk_cache->setEnabled(checked);

}

void CollectionSettingsPage::Load() {

  if (!initialized_model_) {
    if (ui_->list->selectionModel()) {
      disconnect(ui_->list->selectionModel(), SIGNAL(currentRowChanged(QModelIndex, QModelIndex)), this, SLOT(CurrentRowChanged(QModelIndex)));
    }

    ui_->list->setModel(dialog()->collection_directory_model());
    initialized_model_ = true;

    connect(ui_->list->selectionModel(), SIGNAL(currentRowChanged(QModelIndex, QModelIndex)), SLOT(CurrentRowChanged(QModelIndex)));
  }

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->auto_open->setChecked(s.value("auto_open", true).toBool());
  ui_->pretty_covers->setChecked(s.value("pretty_covers", true).toBool());
  ui_->show_dividers->setChecked(s.value("show_dividers", true).toBool());
  ui_->startup_scan->setChecked(s.value("startup_scan", true).toBool());
  ui_->monitor->setChecked(s.value("monitor", true).toBool());
  ui_->mark_songs_unavailable->setChecked(s.value("mark_songs_unavailable", false).toBool());

  QStringList filters = s.value("cover_art_patterns", QStringList() << "front" << "cover").toStringList();
  ui_->cover_art_patterns->setText(filters.join(","));

  ui_->checkbox_cover_album_dir->setChecked(s.value("cover_album_dir", false).toBool());
  SaveCover save_cover = SaveCover(s.value("cover_filename", SaveCover_Hash).toInt());
  switch (save_cover) {
    case SaveCover_Hash: ui_->radiobutton_cover_hash->setChecked(true); break;
    case SaveCover_Pattern: ui_->radiobutton_cover_pattern->setChecked(true); break;
  }
  QString cover_pattern = s.value("cover_pattern").toString();
  if (!cover_pattern.isEmpty()) ui_->lineedit_cover_pattern->setText(cover_pattern);
  ui_->checkbox_cover_overwrite->setChecked(s.value("cover_overwrite", false).toBool());
  ui_->checkbox_cover_lowercase->setChecked(s.value("cover_lowercase", true).toBool());
  ui_->checkbox_cover_replace_spaces->setChecked(s.value("cover_replace_spaces", true).toBool());

  ui_->spinbox_cache_size->setValue(s.value(kSettingsCacheSize, kSettingsCacheSizeDefault).toInt());
  ui_->combobox_cache_size->setCurrentIndex(s.value(kSettingsCacheSizeUnit, static_cast<int>(CacheSizeUnit_MB)).toInt());
  ui_->checkbox_disk_cache->setChecked(s.value(kSettingsDiskCacheEnable, false).toBool());
  ui_->spinbox_disk_cache_size->setValue(s.value(kSettingsDiskCacheSize, kSettingsDiskCacheSizeDefault).toInt());
  ui_->combobox_disk_cache_size->setCurrentIndex(s.value(kSettingsDiskCacheSizeUnit, static_cast<int>(CacheSizeUnit_MB)).toInt());

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  ui_->checkbox_delete_files->setChecked(s.value("delete_files", false).toBool());
#else
  ui_->checkbox_delete_files->setChecked(false);
  ui_->checkbox_delete_files->hide();
#endif

  s.endGroup();

  DiskCacheEnable(ui_->checkbox_disk_cache->checkState());

  ui_->disk_cache_in_use->setText((dialog()->app()->collection_model()->icon_cache_disk_size() == 0 ? "empty" : Utilities::PrettySize(dialog()->app()->collection_model()->icon_cache_disk_size())));

  Init(ui_->layout_collectionsettingspage->parentWidget());
  if (!QSettings().childGroups().contains(kSettingsGroup)) set_changed();

}

void CollectionSettingsPage::Save() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  s.setValue("auto_open", ui_->auto_open->isChecked());
  s.setValue("pretty_covers", ui_->pretty_covers->isChecked());
  s.setValue("show_dividers", ui_->show_dividers->isChecked());
  s.setValue("startup_scan", ui_->startup_scan->isChecked());
  s.setValue("monitor", ui_->monitor->isChecked());
  s.setValue("mark_songs_unavailable", ui_->mark_songs_unavailable->isChecked());

  QString filter_text = ui_->cover_art_patterns->text();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  QStringList filters = filter_text.split(',', Qt::SkipEmptyParts);
#else
  QStringList filters = filter_text.split(',', QString::SkipEmptyParts);
#endif

  s.setValue("cover_art_patterns", filters);

  s.setValue("cover_album_dir", ui_->checkbox_cover_album_dir->isChecked());
  SaveCover save_cover = SaveCover_Hash;
  if (ui_->radiobutton_cover_hash->isChecked()) save_cover = SaveCover_Hash;
  if (ui_->radiobutton_cover_pattern->isChecked()) save_cover = SaveCover_Pattern;
  s.setValue("cover_filename", int(save_cover));
  s.setValue("cover_pattern", ui_->lineedit_cover_pattern->text());
  s.setValue("cover_overwrite", ui_->checkbox_cover_overwrite->isChecked());
  s.setValue("cover_lowercase", ui_->checkbox_cover_lowercase->isChecked());
  s.setValue("cover_replace_spaces", ui_->checkbox_cover_replace_spaces->isChecked());

  s.setValue(kSettingsCacheSize, ui_->spinbox_cache_size->value());
  s.setValue(kSettingsCacheSizeUnit, ui_->combobox_cache_size->currentIndex());
  s.setValue(kSettingsDiskCacheEnable, ui_->checkbox_disk_cache->isChecked());
  s.setValue(kSettingsDiskCacheSize, ui_->spinbox_disk_cache_size->value());
  s.setValue(kSettingsDiskCacheSizeUnit, ui_->combobox_disk_cache_size->currentIndex());

  s.setValue("delete_files", ui_->checkbox_delete_files->isChecked());

  s.endGroup();

}

void CollectionSettingsPage::CoverSaveInAlbumDirChanged() {

  if (ui_->checkbox_cover_album_dir->isChecked()) {
    if (!ui_->groupbox_cover_filename->isEnabled()) {
      ui_->groupbox_cover_filename->setEnabled(true);
    }
    if (ui_->radiobutton_cover_pattern->isChecked()) {
      if (!ui_->lineedit_cover_pattern->isEnabled()) ui_->lineedit_cover_pattern->setEnabled(true);
      if (!ui_->checkbox_cover_overwrite->isEnabled()) ui_->checkbox_cover_overwrite->setEnabled(true);
      if (!ui_->checkbox_cover_lowercase->isEnabled()) ui_->checkbox_cover_lowercase->setEnabled(true);
      if (!ui_->checkbox_cover_replace_spaces->isEnabled()) ui_->checkbox_cover_replace_spaces->setEnabled(true);
    }
    else {
      if (ui_->lineedit_cover_pattern->isEnabled()) ui_->lineedit_cover_pattern->setEnabled(false);
      if (ui_->checkbox_cover_overwrite->isEnabled()) ui_->checkbox_cover_overwrite->setEnabled(false);
      if (ui_->checkbox_cover_lowercase->isEnabled()) ui_->checkbox_cover_lowercase->setEnabled(false);
      if (ui_->checkbox_cover_replace_spaces->isEnabled()) ui_->checkbox_cover_replace_spaces->setEnabled(false);
    }
  }
  else {
    if (ui_->groupbox_cover_filename->isEnabled()) {
      ui_->groupbox_cover_filename->setEnabled(false);
    }
  }

}

void CollectionSettingsPage::ClearPixmapDiskCache() {

  ui_->disk_cache_in_use->setText("empty");

}
