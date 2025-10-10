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

#include "config.h"

#include <utility>
#include <limits>

#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QString>
#include <QStringList>
#include <QStorageInfo>
#include <QFileInfo>
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

#include "constants/filesystemconstants.h"
#include "core/iconloader.h"
#include "core/standardpaths.h"
#include "core/settings.h"
#include "utilities/strutils.h"
#include "collection/collectionlibrary.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectiondirectory.h"
#include "collection/collectiondirectorymodel.h"
#include "collectionsettingspage.h"
#include "collectionsettingsdirectorymodel.h"
#include "playlist/playlistdelegates.h"
#include "settings/settingsdialog.h"
#include "settings/settingspage.h"
#include "constants/collectionsettings.h"
#include "ui_collectionsettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace CollectionSettings;

CollectionSettingsPage::CollectionSettingsPage(SettingsDialog *dialog,
                                               const SharedPtr<CollectionLibrary> collection,
                                               const SharedPtr<CollectionBackend> collection_backend,
                                               CollectionModel *collection_model,
                                               CollectionDirectoryModel *collection_directory_model,
                                               QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_CollectionSettingsPage),
      collection_(collection),
      collection_backend_(collection_backend),
      collection_model_(collection_model),
      collectionsettings_directory_model_(new CollectionSettingsDirectoryModel(this)),
      collection_directory_model_(collection_directory_model),
      initialized_model_(false) {

  ui_->setupUi(this);
  ui_->list->setItemDelegate(new NativeSeparatorsDelegate(this));

  setWindowIcon(IconLoader::Load(u"library-music"_s, true, 0, 32));
  ui_->add_directory->setIcon(IconLoader::Load(u"document-open-folder"_s));

  ui_->combobox_cache_size->addItem(u"KB"_s, static_cast<int>(CacheSizeUnit::KB));
  ui_->combobox_cache_size->addItem(u"MB"_s, static_cast<int>(CacheSizeUnit::MB));

  ui_->combobox_disk_cache_size->addItem(u"KB"_s, static_cast<int>(CacheSizeUnit::KB));
  ui_->combobox_disk_cache_size->addItem(u"MB"_s, static_cast<int>(CacheSizeUnit::MB));
  ui_->combobox_disk_cache_size->addItem(u"GB"_s, static_cast<int>(CacheSizeUnit::GB));

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

  ui_->startup_scan->setChecked(s.value(kStartupScan, true).toBool());
  ui_->monitor->setChecked(s.value(kMonitor, true).toBool());
  ui_->song_tracking->setChecked(s.value(kSongTracking, false).toBool());
  ui_->mark_songs_unavailable->setChecked(ui_->song_tracking->isChecked() ? true : s.value(kMarkSongsUnavailable, true).toBool());
  ui_->song_ebur128_loudness_analysis->setChecked(s.value(kSongENUR128LoudnessAnalysis, false).toBool());
  ui_->expire_unavailable_songs_days->setValue(s.value(kExpireUnavailableSongs, 60).toInt());

  QStringList filters = s.value(kCoverArtPatterns, QStringList() << u"front"_s << u"cover"_s).toStringList();
  ui_->cover_art_patterns->setText(filters.join(u','));

  ui_->auto_open->setChecked(s.value(kAutoOpen, true).toBool());
  ui_->show_dividers->setChecked(s.value(kShowDividers, true).toBool());
  ui_->pretty_covers->setChecked(s.value(kPrettyCovers, true).toBool());
  ui_->various_artists->setChecked(s.value(kVariousArtists, true).toBool());
  ui_->checkbox_skip_articles_for_artists->setChecked(s.value(kSkipArticlesForArtists, true).toBool());
  ui_->checkbox_skip_articles_for_albums->setChecked(s.value(kSkipArticlesForAlbums, false).toBool());

  ui_->spinbox_cache_size->setValue(s.value(kSettingsCacheSize, kSettingsCacheSizeDefault).toInt());
  ui_->combobox_cache_size->setCurrentIndex(ui_->combobox_cache_size->findData(s.value(kSettingsCacheSizeUnit, static_cast<int>(CacheSizeUnit::MB)).toInt()));
  ui_->checkbox_disk_cache->setChecked(s.value(kSettingsDiskCacheEnable, false).toBool());
  ui_->spinbox_disk_cache_size->setValue(s.value(kSettingsDiskCacheSize, kSettingsDiskCacheSizeDefault).toInt());
  ui_->combobox_disk_cache_size->setCurrentIndex(ui_->combobox_disk_cache_size->findData(s.value(kSettingsDiskCacheSizeUnit, static_cast<int>(CacheSizeUnit::MB)).toInt()));

  ui_->checkbox_save_playcounts->setChecked(s.value(kSavePlayCounts, false).toBool());
  ui_->checkbox_save_ratings->setChecked(s.value(kSaveRatings, false).toBool());
  ui_->checkbox_overwrite_playcount->setChecked(s.value(kOverwritePlaycount, false).toBool());
  ui_->checkbox_overwrite_rating->setChecked(s.value(kOverwriteRating, false).toBool());

  ui_->checkbox_delete_files->setChecked(s.value(kDeleteFiles, false).toBool());

  s.endGroup();

  DiskCacheEnable(ui_->checkbox_disk_cache->checkState());

  UpdateIconDiskCacheSize();

  Init(ui_->layout_collectionsettingspage->parentWidget());
  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void CollectionSettingsPage::Save() {

  Settings s;

  s.beginGroup(kSettingsGroup);

  s.setValue(kStartupScan, ui_->startup_scan->isChecked());
  s.setValue(kMonitor, ui_->monitor->isChecked());
  s.setValue(kSongTracking, ui_->song_tracking->isChecked());
  s.setValue(kMarkSongsUnavailable, ui_->song_tracking->isChecked() ? true : ui_->mark_songs_unavailable->isChecked());
  s.setValue(kSongENUR128LoudnessAnalysis, ui_->song_ebur128_loudness_analysis->isChecked());
  s.setValue(kExpireUnavailableSongs, ui_->expire_unavailable_songs_days->value());

  const QString filter_text = ui_->cover_art_patterns->text();
  s.setValue(kCoverArtPatterns, filter_text.split(u',', Qt::SkipEmptyParts));

  s.setValue(kAutoOpen, ui_->auto_open->isChecked());
  s.setValue(kShowDividers, ui_->show_dividers->isChecked());
  s.setValue(kPrettyCovers, ui_->pretty_covers->isChecked());
  s.setValue(kVariousArtists, ui_->various_artists->isChecked());
  s.setValue(kSkipArticlesForArtists, ui_->checkbox_skip_articles_for_artists->isChecked());
  s.setValue(kSkipArticlesForAlbums, ui_->checkbox_skip_articles_for_albums->isChecked());

  s.setValue(kSettingsCacheSize, ui_->spinbox_cache_size->value());
  s.setValue(kSettingsCacheSizeUnit, ui_->combobox_cache_size->currentData().toInt());
  s.setValue(kSettingsDiskCacheEnable, ui_->checkbox_disk_cache->isChecked());
  s.setValue(kSettingsDiskCacheSize, ui_->spinbox_disk_cache_size->value());
  s.setValue(kSettingsDiskCacheSizeUnit, ui_->combobox_disk_cache_size->currentData().toInt());

  s.setValue(kSavePlayCounts, ui_->checkbox_save_playcounts->isChecked());
  s.setValue(kSaveRatings, ui_->checkbox_save_ratings->isChecked());
  s.setValue(kOverwritePlaycount, ui_->checkbox_overwrite_playcount->isChecked());
  s.setValue(kOverwriteRating, ui_->checkbox_overwrite_rating->isChecked());

  s.setValue(kDeleteFiles, ui_->checkbox_delete_files->isChecked());

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

  QString path = s.value(kLastPath, StandardPaths::WritableLocation(StandardPaths::StandardLocation::MusicLocation)).toString();
  path = QDir::cleanPath(QFileDialog::getExistingDirectory(this, tr("Add directory..."), path));

  if (!path.isEmpty()) {
    const QByteArray filesystemtype = QStorageInfo(QFileInfo(path).canonicalFilePath()).fileSystemType();
    if (kRejectedFileSystems.contains(filesystemtype)) {
      QMessageBox messagebox(QMessageBox::Critical, QObject::tr("Invalid collection directory"), QObject::tr("Can't add directory %1 with special filesystem %2 to collection").arg(path).arg(QString::fromUtf8(filesystemtype)));
      (void)messagebox.exec();
      return;
    }
    collectionsettings_directory_model_->AddDirectory(path);
  }

  s.setValue(kLastPath, path);

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

  collection_model_->ClearIconDiskCache();

  UpdateIconDiskCacheSize();

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

void CollectionSettingsPage::UpdateIconDiskCacheSize() {

  ui_->disk_cache_in_use->setText(collection_model_->icon_disk_cache_size() == 0 ? u"empty"_s : Utilities::PrettySize(collection_model_->icon_disk_cache_size()));

}

void CollectionSettingsPage::WriteAllSongsStatisticsToFiles() {

  QMessageBox confirmation_dialog(QMessageBox::Question, tr("Write all playcounts and ratings to files"), tr("Are you sure you want to write song playcounts and ratings to file for all songs in your collection?"), QMessageBox::Yes | QMessageBox::Cancel);
  if (confirmation_dialog.exec() != QMessageBox::Yes) {
    return;
  }

  collection_->SyncPlaycountAndRatingToFilesAsync();

}
