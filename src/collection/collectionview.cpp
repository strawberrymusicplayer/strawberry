/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QtGlobal>
#include <QWidget>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QList>
#include <QMultiMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QSettings>

#include "core/iconloader.h"
#include "core/musicstorage.h"
#include "core/deletefiles.h"
#include "core/settings.h"
#include "utilities/filemanagerutils.h"
#include "collectionmanager.h"
#include "collectionbackend.h"
#include "collectiondirectorymodel.h"
#include "collectionmodel.h"
#include "collectionview.h"
#include "device/devicemanager.h"
#include "device/devicestatefiltermodel.h"
#include "dialogs/edittagdialog.h"
#include "dialogs/deleteconfirmationdialog.h"
#include "organize/organizedialog.h"
#include "organize/organizeerrordialog.h"
#include "constants/collectionsettings.h"

using std::make_unique;
using namespace Qt::Literals::StringLiterals;

CollectionView::CollectionView(QWidget *parent)
    : CollectionTreeView(parent, -1),
      total_artist_count_(-1),
      total_album_count_(-1),
      action_organize_(nullptr),
      action_copy_to_device_(nullptr),
      action_edit_track_(nullptr),
      action_edit_tracks_(nullptr),
      action_rescan_songs_(nullptr),
      action_show_in_browser_(nullptr),
      action_show_in_various_(nullptr),
      action_no_show_in_various_(nullptr),
      action_delete_files_(nullptr),
      delete_files_(false) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

}

CollectionView::~CollectionView() = default;

void CollectionView::Init(const SharedPtr<TaskManager> task_manager,
                          const SharedPtr<TagReaderClient> tagreader_client,
                          const SharedPtr<NetworkAccessManager> network,
                          const SharedPtr<AlbumCoverLoader> albumcover_loader,
                          const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
                          const SharedPtr<CoverProviders> cover_providers,
                          const SharedPtr<LyricsProviders> lyrics_providers,
                          const SharedPtr<CollectionManager> collection,
                          const SharedPtr<DeviceManager> device_manager,
                          const SharedPtr<StreamingServices> streaming_services) {

  task_manager_ = task_manager;
  tagreader_client_ = tagreader_client;
  network_ = network;
  albumcover_loader_ = albumcover_loader;
  current_albumcover_loader_ = current_albumcover_loader;
  cover_providers_ = cover_providers;
  lyrics_providers_ = lyrics_providers;
  collection_ = collection;
  device_manager_ = device_manager;
  streaming_services_ = streaming_services;
  backend_ = collection_->backend();
  SetCollectionModel(collection_->model());

  ReloadSettings();

}

void CollectionView::ReloadSettings() {

  Settings settings;
  settings.beginGroup(CollectionSettings::kSettingsGroup);
  SetAutoOpen(settings.value(CollectionSettings::kAutoOpen, CollectionSettings::kDefaultAutoOpen).toBool());
  delete_files_ = settings.value(CollectionSettings::kDeleteFiles, CollectionSettings::kDefaultDeleteFiles).toBool();
  settings.endGroup();

}

void CollectionView::TotalArtistCountUpdated(const int count) {

  total_artist_count_ = count;
  Q_EMIT TotalArtistCountUpdated_();

}

void CollectionView::TotalAlbumCountUpdated(const int count) {

  total_album_count_ = count;
  Q_EMIT TotalAlbumCountUpdated_();

}

QString CollectionView::EmptyTitleText() const {
  return tr("Your collection is empty!");
}

QString CollectionView::EmptyText() const {
  return tr("Click here to add some music");
}

void CollectionView::EmptyClicked() {
  Q_EMIT ShowSettingsDialog();
}

void CollectionView::AddContextMenuActions(QMenu *menu) {

  action_organize_ = menu->addAction(IconLoader::Load(u"edit-copy"_s), tr("Organize files..."), this, &CollectionView::Organize);
  action_copy_to_device_ = menu->addAction(IconLoader::Load(u"device"_s), tr("Copy to device..."), this, &CollectionView::CopyToDevice);
  action_delete_files_ = menu->addAction(IconLoader::Load(u"edit-delete"_s), tr("Delete from disk..."), this, &CollectionView::Delete);

  menu->addSeparator();
  action_edit_track_ = menu->addAction(IconLoader::Load(u"edit-rename"_s), tr("Edit track information..."), this, &CollectionView::EditTracks);
  action_edit_tracks_ = menu->addAction(IconLoader::Load(u"edit-rename"_s), tr("Edit tracks information..."), this, &CollectionView::EditTracks);
  action_show_in_browser_ = menu->addAction(IconLoader::Load(u"document-open-folder"_s), tr("Show in file browser..."), this, &CollectionView::ShowInBrowser);

  menu->addSeparator();

  action_rescan_songs_ = menu->addAction(tr("Rescan song(s)"), this, &CollectionView::RescanSongs);

  menu->addSeparator();
  action_show_in_various_ = menu->addAction(tr("Show in various artists"), this, &CollectionView::ShowInVarious);
  action_no_show_in_various_ = menu->addAction(tr("Don't show in various artists"), this, &CollectionView::NoShowInVarious);

  menu->addSeparator();

  action_copy_to_device_->setDisabled(device_manager_->connected_devices_model()->rowCount() == 0);
  QObject::connect(device_manager_->connected_devices_model(), &DeviceStateFilterModel::IsEmptyChanged, action_copy_to_device_, &QAction::setDisabled);

}

void CollectionView::UpdateContextMenuActions(const bool has_selection) {

  int regular_elements = 0;
  int regular_editable = 0;

  if (filter_model() && selectionModel()) {
    const QModelIndexList selected_indexes = filter_model()->mapSelectionToSource(selectionModel()->selection()).indexes();
    for (const QModelIndex &idx : selected_indexes) {
      ++regular_elements;
      if (collection_model()->data(idx, CollectionModel::Role_Editable).toBool()) {
        ++regular_editable;
      }
    }
  }

  // If neither edit_track not edit_tracks are available, we show disabled edit_track element
  action_edit_track_->setVisible(regular_editable == 1);
  action_edit_track_->setEnabled(regular_editable == 1);
  action_edit_tracks_->setVisible(regular_editable > 1);
  action_edit_tracks_->setEnabled(regular_editable > 1);

  action_rescan_songs_->setVisible(regular_editable > 0);
  action_rescan_songs_->setEnabled(regular_editable > 0);

  // Only when all selected items are editable
  action_organize_->setVisible(regular_elements == regular_editable);
  action_organize_->setEnabled(regular_elements == regular_editable);
  action_copy_to_device_->setVisible(regular_elements == regular_editable);
  // Also needs a connected device, IsEmptyChanged keeps it disabled while there is none.
  action_copy_to_device_->setEnabled(regular_elements == regular_editable && device_manager_->connected_devices_model()->rowCount() > 0);

  action_delete_files_->setVisible(delete_files_);
  action_delete_files_->setEnabled(delete_files_);

  action_show_in_various_->setVisible(has_selection);
  action_no_show_in_various_->setVisible(has_selection);

}

void CollectionView::ShowInVarious() { SetShowInVarious(true); }

void CollectionView::NoShowInVarious() { SetShowInVarious(false); }

void CollectionView::SetShowInVarious(const bool on) {

  if (!context_menu_index().isValid()) return;

  // Map is from album name -> all artists sharing that album name, built from each selected song.
  // We put through "Various Artists" changes one album at a time,
  // to make sure the old album node gets removed (due to all children removed), before the new one gets added
  QMultiMap<QString, QString> albums;
  const SongList songs = GetSelectedSongs();
  for (const Song &song : songs) {
    if (albums.find(song.album(), song.artist()) == albums.end())
      albums.insert(song.album(), song.artist());
  }

  // If we have only one album and we are putting it into Various Artists, check to see
  // if there are other Artists in this album and prompt the user if they'd like them moved, too
  if (on && albums.size() == 1) {
    const QString album = albums.cbegin().key();
    const SongList all_of_album = backend_->GetSongsByAlbum(album);
    QSet<QString> other_artists;
    for (const Song &s : all_of_album) {
      if (!albums.contains(album, s.artist()) && !other_artists.contains(s.artist())) {
        other_artists.insert(s.artist());
      }
    }
    if (other_artists.count() > 0) {
      if (QMessageBox::question(this, tr("There are other songs in this album"), tr("Would you like to move the other songs on this album to Various Artists as well?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
        for (const QString &s : other_artists) {
          albums.insert(album, s);
        }
      }
    }
  }

  const QSet<QString> albums_set = QSet<QString>(albums.keyBegin(), albums.keyEnd());
  for (const QString &album : albums_set) {
    backend_->ForceCompilation(album, albums.values(album), on);
  }

}

void CollectionView::Organize() {

  if (!organize_dialog_) {
    organize_dialog_ = make_unique<OrganizeDialog>(task_manager_, tagreader_client_, backend_, this);
  }

  organize_dialog_->SetDestinationModel(collection_model()->directory_model());
  organize_dialog_->SetCopy(false);
  const SongList songs = GetSelectedSongs();
  if (organize_dialog_->SetSongs(songs)) {
    organize_dialog_->show();
  }
  else {
    QMessageBox::warning(this, tr("Error"), tr("None of the selected songs were suitable for copying to a device"));
  }

}

void CollectionView::EditTracks() {

  if (!edit_tag_dialog_) {
    edit_tag_dialog_ = make_unique<EditTagDialog>(network_, tagreader_client_, backend_, albumcover_loader_, current_albumcover_loader_, cover_providers_, lyrics_providers_, streaming_services_, this);
    QObject::connect(&*edit_tag_dialog_, &EditTagDialog::Error, this, &CollectionView::EditTagError);
  }
  const SongList songs = GetSelectedSongs();
  edit_tag_dialog_->SetSongs(songs);
  edit_tag_dialog_->show();

}

void CollectionView::EditTagError(const QString &message) {
  Q_EMIT Error(message);
}

void CollectionView::RescanSongs() {

  collection_->Rescan(GetSelectedSongs());

}

void CollectionView::CopyToDevice() {

  if (!organize_dialog_) {
    organize_dialog_ = make_unique<OrganizeDialog>(task_manager_, tagreader_client_, nullptr, this);
  }

  organize_dialog_->SetDestinationModel(device_manager_->connected_devices_model(), true);
  organize_dialog_->SetCopy(true);
  organize_dialog_->SetSongs(GetSelectedSongs());
  organize_dialog_->show();

}

void CollectionView::ShowInBrowser() const {

  const SongList songs = GetSelectedSongs();
  QList<QUrl> urls;
  urls.reserve(songs.count());
  for (const Song &song : songs) {
    urls << song.url();
  }

  Utilities::OpenInFileBrowser(urls);

}

void CollectionView::Delete() {

  if (!delete_files_) return;

  const SongList selected_songs = GetSelectedSongs();

  SongList songs;
  QStringList files;
  songs.reserve(selected_songs.count());
  files.reserve(selected_songs.count());
  for (const Song &song : selected_songs) {
    QString filename = song.url().toLocalFile();
    if (!files.contains(filename)) {
      songs << song;
      files << filename;
    }
  }
  if (DeleteConfirmationDialog::warning(files) != QDialogButtonBox::Yes) return;

  // We can cheat and always take the storage of the first directory, since they'll all be FilesystemMusicStorage in a collection and deleting doesn't check the actual directory.
  SharedPtr<MusicStorage> storage = collection_model()->directory_model()->index(0, 0).data(MusicStorage::Role_Storage).value<SharedPtr<MusicStorage>>();

  DeleteFiles *delete_files = new DeleteFiles(task_manager_, storage, true);
  QObject::connect(delete_files, &DeleteFiles::Finished, this, &CollectionView::DeleteFilesFinished);
  delete_files->Start(songs);

}

void CollectionView::DeleteFilesFinished(const SongList &songs_with_errors) {

  if (songs_with_errors.isEmpty()) return;

  OrganizeErrorDialog *dialog = new OrganizeErrorDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->Show(OrganizeErrorDialog::OperationType::Delete, songs_with_errors);

}
