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

#ifndef COLLECTIONVIEW_H
#define COLLECTIONVIEW_H

#include "config.h"

#include <QObject>
#include <QString>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "collectiontreeview.h"

class QWidget;
class QMenu;
class QAction;

class TaskManager;
class TagReaderClient;
class NetworkAccessManager;
class CollectionManager;
class CollectionBackend;
class DeviceManager;
class StreamingServices;
class AlbumCoverLoader;
class CurrentAlbumCoverLoader;
class CoverProviders;
class LyricsProviders;
class EditTagDialog;
class OrganizeDialog;

class CollectionView : public CollectionTreeView {
  Q_OBJECT

 public:
  explicit CollectionView(QWidget *parent = nullptr);
  ~CollectionView() override;

  void Init(const SharedPtr<TaskManager> task_manager,
            const SharedPtr<TagReaderClient> tagreader_client,
            const SharedPtr<NetworkAccessManager> network,
            const SharedPtr<AlbumCoverLoader> albumcover_loader,
            const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
            const SharedPtr<CoverProviders> cover_providers,
            const SharedPtr<LyricsProviders> lyrics_providers,
            const SharedPtr<CollectionManager> collection,
            const SharedPtr<DeviceManager> device_manager,
            const SharedPtr<StreamingServices> streaming_services);

  int TotalArtists() const { return total_artist_count_; }
  int TotalAlbums() const { return total_album_count_; }

 public Q_SLOTS:
  void TotalArtistCountUpdated(const int count);
  void TotalAlbumCountUpdated(const int count);
  void ReloadSettings();

  void EditTagError(const QString &message);

 Q_SIGNALS:
  void ShowSettingsDialog();

  void TotalArtistCountUpdated_();
  void TotalAlbumCountUpdated_();
  void Error(const QString &error);

 protected:
  QString EmptyTitleText() const override;
  QString EmptyText() const override;
  void EmptyClicked() override;
  void AddContextMenuActions(QMenu *menu) override;
  void UpdateContextMenuActions(const bool has_selection) override;

 private Q_SLOTS:
  void Organize();
  void CopyToDevice();
  void EditTracks();
  void RescanSongs();
  void ShowInBrowser() const;
  void ShowInVarious();
  void NoShowInVarious();
  void Delete();
  void DeleteFilesFinished(const SongList &songs_with_errors);

 private:
  void SetShowInVarious(const bool on);

 private:
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<NetworkAccessManager> network_;
  SharedPtr<DeviceManager> device_manager_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  SharedPtr<CollectionManager> collection_;
  SharedPtr<CoverProviders> cover_providers_;
  SharedPtr<LyricsProviders> lyrics_providers_;
  SharedPtr<StreamingServices> streaming_services_;

  SharedPtr<CollectionBackend> backend_;

  int total_artist_count_;
  int total_album_count_;

  QAction *action_organize_;
  QAction *action_copy_to_device_;
  QAction *action_edit_track_;
  QAction *action_edit_tracks_;
  QAction *action_rescan_songs_;
  QAction *action_show_in_browser_;
  QAction *action_show_in_various_;
  QAction *action_no_show_in_various_;
  QAction *action_delete_files_;

  ScopedPtr<OrganizeDialog> organize_dialog_;
  ScopedPtr<EditTagDialog> edit_tag_dialog_;

  bool delete_files_;
};

#endif  // COLLECTIONVIEW_H
