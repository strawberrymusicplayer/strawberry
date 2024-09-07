/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>
#include <limits>

#include <QStandardPaths>
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QString>
#include <QStringList>
#include <QDir>
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
#include <QMessageBox>

#include "core/application.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "collection/collection.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectiondirectory.h"
#include "collection/collectiondirectorymodel.h"
#include "collectionsettingspage.h"
#include "collectionsettingsdirectorymodel.h"
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

CollectionSettingsPage::CollectionSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_CollectionSettingsPage),
      collection_backend_(dialog->app()->collection_backend()),
      collectionsettings_directory_model_(new CollectionSettingsDirectoryModel(this)),
      collection_directory_model_(dialog->collection_directory_model()),
      initialized_model_(false) {

  ui_->setupUi(this);
  ui_->list->setItemDelegate(new NativeSeparatorsDelegate(this));

  // Icons
  setWindowIcon(IconLoader::Load(QStringLiteral("library-music"), true, 0, 32));
  ui_->add_directory->setIcon(IconLoader::Load(QStringLiteral("document-open-folder")));

  ui_->combobox_cache_size->addItem(QStringLiteral("KB"), static_cast<int>(CacheSizeUnit::KB));
  ui_->combobox_cache_size->addItem(QStringLiteral("MB"), static_cast<int>(CacheSizeUnit::MB));

  ui_->combobox_disk_cache_size->addItem(QStringLiteral("KB"), static_cast<int>(CacheSizeUnit::KB));
  ui_->combobox_disk_cache_size->addItem(QStringLiteral("MB"), static_cast<int>(CacheSizeUnit::MB));
  ui_->combobox_disk_cache_size->addItem(QStringLiteral("GB"), static_cast<int>(CacheSizeUnit::GB));

  QObject::connect(ui_->add_directory, &QPushButton::clicked, this, &CollectionSettingsPage::AddDirectory);
  QObject::connect(ui_->remove_directory, &QPushButton::clicked, this, &CollectionSettingsPage::RemoveDirectory);

#ifdef HAVE_SONGFINGERPRINTING
  QObject::connect(ui_->song_tracking, &QCheckBox::toggled, this, &CollectionSettingsPage::SongTrackingToggled);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  QObject::connect(ui_->checkbox_disk_cache, &QCheckBox::checkStateChanged, this, &CollectionSettingsPage::DiskCacheEnable);
#else
  QObject::connect(ui_->checkbox_disk_cache, &QCheckBox::stateChanged, this, &CollectionSettingsPage::DiskCacheEnable);
#endif
  QObject::connect(ui_->button_clear_disk_cache, &QPushButton::clicked, dialog->app(), &Application::ClearPixmapDiskCache);
  QObject::connect(ui_->button_clear_disk_cache, &QPushButton::clicked, this, &CollectionSettingsPage::ClearPixmapDiskCache);

  QObject::connect(ui_->combobox_cache_size, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CollectionSettingsPage::CacheSizeUnitChanged);
  QObject::connect(ui_->combobox_disk_cache_size, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CollectionSettingsPage::DiskCacheSizeUnitChanged);

  QObject::connect(ui_->button_save_stats, &QPushButton::clicked, this, &CollectionSettingsPage::WriteAllSongsStatisticsToFiles);

#ifndef HAVE_SONGFINGERPRINTING
  ui_->song_tracking->hide();
#endif

#ifndef HAVE_EBUR128
  ui_->song_ebur128_loudness_analysis->hide();
#endif

}

CollectionSettingsPage::~CollectionSettingsPage() { delete ui_; }

void CollectionSettingsPage::Load() {

  if (!initialized_model_) {
    if (ui_->list->selectionModel()) {
      QObject::disconnect(ui_->list->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &CollectionSettingsPage::CurrentRowChanged);
    }

    ui_->list->setModel(collectionsettings_directory_model_);
    initialized_model_ = true;

    QObject::connect(ui_->list->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &CollectionSettingsPage::CurrentRowChanged);
  }

  ui_->list->model()->removeRows(0, ui_->list->model()->rowCount());
  const QStringList paths = collection_directory_model_->paths();
  for (const QString &path : paths) {
    collectionsettings_directory_model_->AddDirectory(path);
  }

  Settings s;

  s.beginGroup(kSettingsGroup);
  ui_->auto_open->setChecked(s.value("auto_open", true).toBool());
  ui_->show_dividers->setChecked(s.value("show_dividers", true).toBool());
  ui_->pretty_covers->setChecked(s.value("pretty_covers", true).toBool());
  ui_->various_artists->setChecked(s.value("various_artists", true).toBool());
  ui_->sort_skips_articles->setChecked(s.value("sort_skips_articles", true).toBool());
  ui_->startup_scan->setChecked(s.value("startup_scan", true).toBool());
  ui_->monitor->setChecked(s.value("monitor", true).toBool());
  ui_->song_tracking->setChecked(s.value("song_tracking", false).toBool());
  ui_->song_ebur128_loudness_analysis->setChecked(s.value("song_ebur128_loudness_analysis", false).toBool());
  ui_->mark_songs_unavailable->setChecked(ui_->song_tracking->isChecked() ? true : s.value("mark_songs_unavailable", true).toBool());
  ui_->expire_unavailable_songs_days->setValue(s.value("expire_unavailable_songs", 60).toInt());

  QStringList filters = s.value("cover_art_patterns", QStringList() << QStringLiteral("front") << QStringLiteral("cover")).toStringList();
  ui_->cover_art_patterns->setText(filters.join(u','));

  ui_->spinbox_cache_size->setValue(s.value(kSettingsCacheSize, kSettingsCacheSizeDefault).toInt());
  ui_->combobox_cache_size->setCurrentIndex(ui_->combobox_cache_size->findData(s.value(kSettingsCacheSizeUnit, static_cast<int>(CacheSizeUnit::MB)).toInt()));
  ui_->checkbox_disk_cache->setChecked(s.value(kSettingsDiskCacheEnable, false).toBool());
  ui_->spinbox_disk_cache_size->setValue(s.value(kSettingsDiskCacheSize, kSettingsDiskCacheSizeDefault).toInt());
  ui_->combobox_disk_cache_size->setCurrentIndex(ui_->combobox_disk_cache_size->findData(s.value(kSettingsDiskCacheSizeUnit, static_cast<int>(CacheSizeUnit::MB)).toInt()));

  ui_->checkbox_save_playcounts->setChecked(s.value("save_playcounts", false).toBool());
  ui_->checkbox_save_ratings->setChecked(s.value("save_ratings", false).toBool());
  ui_->checkbox_overwrite_playcount->setChecked(s.value("overwrite_playcount", false).toBool());
  ui_->checkbox_overwrite_rating->setChecked(s.value("overwrite_rating", false).toBool());

  ui_->checkbox_delete_files->setChecked(s.value("delete_files", false).toBool());

  s.endGroup();

  DiskCacheEnable(ui_->checkbox_disk_cache->checkState());

  ui_->disk_cache_in_use->setText((dialog()->app()->collection_model()->icon_cache_disk_size() == 0 ? QStringLiteral("empty") : Utilities::PrettySize(dialog()->app()->collection_model()->icon_cache_disk_size())));

  Init(ui_->layout_collectionsettingspage->parentWidget());
  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void CollectionSettingsPage::Save() {

  Settings s;

  s.beginGroup(kSettingsGroup);
  s.setValue("auto_open", ui_->auto_open->isChecked());
  s.setValue("show_dividers", ui_->show_dividers->isChecked());
  s.setValue("pretty_covers", ui_->pretty_covers->isChecked());
  s.setValue("various_artists", ui_->various_artists->isChecked());
  s.setValue("sort_skips_articles", ui_->sort_skips_articles->isChecked());
  s.setValue("startup_scan", ui_->startup_scan->isChecked());
  s.setValue("monitor", ui_->monitor->isChecked());
  s.setValue("song_tracking", ui_->song_tracking->isChecked());
  s.setValue("song_ebur128_loudness_analysis", ui_->song_ebur128_loudness_analysis->isChecked());
  s.setValue("mark_songs_unavailable", ui_->song_tracking->isChecked() ? true : ui_->mark_songs_unavailable->isChecked());
  s.setValue("expire_unavailable_songs", ui_->expire_unavailable_songs_days->value());

  QString filter_text = ui_->cover_art_patterns->text();

  const QStringList filters = filter_text.split(u',', Qt::SkipEmptyParts);

  s.setValue("cover_art_patterns", filters);

  s.setValue(kSettingsCacheSize, ui_->spinbox_cache_size->value());
  s.setValue(kSettingsCacheSizeUnit, ui_->combobox_cache_size->currentData().toInt());
  s.setValue(kSettingsDiskCacheEnable, ui_->checkbox_disk_cache->isChecked());
  s.setValue(kSettingsDiskCacheSize, ui_->spinbox_disk_cache_size->value());
  s.setValue(kSettingsDiskCacheSizeUnit, ui_->combobox_disk_cache_size->currentData().toInt());

  s.setValue("save_playcounts", ui_->checkbox_save_playcounts->isChecked());
  s.setValue("save_ratings", ui_->checkbox_save_ratings->isChecked());
  s.setValue("overwrite_playcount", ui_->checkbox_overwrite_playcount->isChecked());
  s.setValue("overwrite_rating", ui_->checkbox_overwrite_rating->isChecked());

  s.setValue("delete_files", ui_->checkbox_delete_files->isChecked());

  s.endGroup();

  const QMap<int, CollectionDirectory> dirs = collection_directory_model_->directories();
  for (const CollectionDirectory &dir : dirs) {
    if (!collectionsettings_directory_model_->paths().contains(dir.path)) {
      collection_backend_->RemoveDirectoryAsync(dir);
    }
  }

  const QStringList paths = collectionsettings_directory_model_->paths();
  for (const QString &path : paths) {
    if (!collection_directory_model_->paths().contains(path)) {
      collection_backend_->AddDirectoryAsync(path);
    }
  }

}

void CollectionSettingsPage::AddDirectory() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  QString path = s.value("last_path", QStandardPaths::writableLocation(QStandardPaths::MusicLocation)).toString();
  path = QDir::cleanPath(QFileDialog::getExistingDirectory(this, tr("Add directory..."), path));

  if (!path.isEmpty()) {
    collectionsettings_directory_model_->AddDirectory(path);
  }

  s.setValue("last_path", path);

  set_changed();

}

void CollectionSettingsPage::RemoveDirectory() {

  collectionsettings_directory_model_->RemoveDirectory(ui_->list->currentIndex());

  set_changed();

}

void CollectionSettingsPage::CurrentRowChanged(const QModelIndex &idx) {
  ui_->remove_directory->setEnabled(idx.isValid());
}

void CollectionSettingsPage::SongTrackingToggled() {

  ui_->mark_songs_unavailable->setEnabled(!ui_->song_tracking->isChecked());
  if (ui_->song_tracking->isChecked()) {
    ui_->mark_songs_unavailable->setChecked(true);
  }

}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void CollectionSettingsPage::DiskCacheEnable(const Qt::CheckState state) {
#else
void CollectionSettingsPage::DiskCacheEnable(const int state) {
#endif

  const bool checked = state == Qt::Checked;
  ui_->label_disk_cache_size->setEnabled(checked);
  ui_->spinbox_disk_cache_size->setEnabled(checked);
  ui_->combobox_disk_cache_size->setEnabled(checked);
  ui_->label_disk_cache_in_use->setEnabled(checked);
  ui_->disk_cache_in_use->setEnabled(checked);
  ui_->button_clear_disk_cache->setEnabled(checked);

}

void CollectionSettingsPage::ClearPixmapDiskCache() {

  ui_->disk_cache_in_use->setText(QStringLiteral("empty"));

}

void CollectionSettingsPage::CacheSizeUnitChanged(int index) {

  const CacheSizeUnit cache_size_unit = static_cast<CacheSizeUnit>(ui_->combobox_cache_size->currentData(index).toInt());

  switch (cache_size_unit) {
    case CacheSizeUnit::MB:
      ui_->spinbox_cache_size->setMaximum(std::numeric_limits<int>::max() / 1024);
      break;
    default:
      ui_->spinbox_cache_size->setMaximum(std::numeric_limits<int>::max());
      break;
  }

}

void CollectionSettingsPage::DiskCacheSizeUnitChanged(int index) {

  const CacheSizeUnit cache_size_unit = static_cast<CacheSizeUnit>(ui_->combobox_disk_cache_size->currentData(index).toInt());

  switch (cache_size_unit) {
    case CacheSizeUnit::GB:
      ui_->spinbox_disk_cache_size->setMaximum(4);
      break;
    default:
      ui_->spinbox_disk_cache_size->setMaximum(std::numeric_limits<int>::max());
      break;
  }

}

void CollectionSettingsPage::WriteAllSongsStatisticsToFiles() {

  QMessageBox confirmation_dialog(QMessageBox::Question, tr("Write all playcounts and ratings to files"), tr("Are you sure you want to write song playcounts and ratings to file for all songs in your collection?"), QMessageBox::Yes | QMessageBox::Cancel);
  if (confirmation_dialog.exec() != QMessageBox::Yes) {
    return;
  }

  dialog()->app()->collection()->SyncPlaycountAndRatingToFilesAsync();

}
