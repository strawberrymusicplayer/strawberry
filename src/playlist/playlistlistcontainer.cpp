/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <memory>

#include <QObject>
#include <QWidget>
#include <QStandardItemModel>
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QSet>
#include <QList>
#include <QString>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QSize>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QToolButton>
#include <QShowEvent>
#include <QContextMenuEvent>
#include <QMimeData>

#include "core/application.h"
#include "core/iconloader.h"
#include "core/player.h"
#include "core/settings.h"
#include "playlist.h"
#include "playlistbackend.h"
#include "playlistlistview.h"
#include "playlistlistcontainer.h"
#include "playlistlistmodel.h"
#include "playlistlistsortfiltermodel.h"
#include "playlistmanager.h"
#include "ui_playlistlistcontainer.h"
#include "organize/organizedialog.h"
#include "settings/appearancesettingspage.h"
#ifndef Q_OS_WIN
#  include "device/devicemanager.h"
#  include "device/devicestatefiltermodel.h"
#endif

using std::make_unique;

PlaylistListContainer::PlaylistListContainer(QWidget *parent)
    : QWidget(parent),
      app_(nullptr),
      ui_(new Ui_PlaylistListContainer),
      menu_(nullptr),
      action_new_folder_(new QAction(this)),
      action_remove_(new QAction(this)),
      action_save_playlist_(new QAction(this)),
#ifndef Q_OS_WIN
      action_copy_to_device_(new QAction(this)),
#endif
      model_(new PlaylistListModel(this)),
      proxy_(new PlaylistListSortFilterModel(this)),
      loaded_icons_(false),
      active_playlist_id_(-1) {

  ui_->setupUi(this);
  ui_->tree->setAttribute(Qt::WA_MacShowFocusRect, false);

  action_new_folder_->setText(tr("New folder"));
  action_remove_->setText(tr("Delete"));
  action_save_playlist_->setText(tr("Save playlist", "Save playlist menu action."));
#ifndef Q_OS_WIN
  action_copy_to_device_->setText(tr("Copy to device..."));
#endif

  ui_->new_folder->setDefaultAction(action_new_folder_);
  ui_->remove->setDefaultAction(action_remove_);
  ui_->save_playlist->setDefaultAction(action_save_playlist_);

  QObject::connect(action_new_folder_, &QAction::triggered, this, &PlaylistListContainer::NewFolderClicked);
  QObject::connect(action_remove_, &QAction::triggered, this, &PlaylistListContainer::Delete);
  QObject::connect(action_save_playlist_, &QAction::triggered, this, &PlaylistListContainer::SavePlaylist);
#ifndef Q_OS_WIN
  QObject::connect(action_copy_to_device_, &QAction::triggered, this, &PlaylistListContainer::CopyToDevice);
#endif
  QObject::connect(model_, &PlaylistListModel::PlaylistPathChanged, this, &PlaylistListContainer::PlaylistPathChanged);

  proxy_->setSourceModel(model_);
  proxy_->setDynamicSortFilter(true);
  proxy_->sort(0);
  ui_->tree->setModel(proxy_);

  QObject::connect(ui_->tree, &PlaylistListView::ItemsSelectedChanged, this, &PlaylistListContainer::ItemsSelectedChanged);
  QObject::connect(ui_->tree, &PlaylistListView::doubleClicked, this, &PlaylistListContainer::ItemDoubleClicked);
  QObject::connect(ui_->tree, &PlaylistListView::ItemMimeDataDroppedSignal, this, &PlaylistListContainer::ItemMimeDataDropped);

  model_->invisibleRootItem()->setData(PlaylistListModel::Type_Folder, PlaylistListModel::Role_Type);

  ReloadSettings();

}

PlaylistListContainer::~PlaylistListContainer() { delete ui_; }

void PlaylistListContainer::SetApplication(Application *app) {

  app_ = app;
  PlaylistManager *manager = &*app_->playlist_manager();
  Player *player = &*app_->player();

  QObject::connect(manager, &PlaylistManager::PlaylistAdded, this, &PlaylistListContainer::AddPlaylist);
  QObject::connect(manager, &PlaylistManager::PlaylistFavorited, this, &PlaylistListContainer::PlaylistFavoriteStateChanged);
  QObject::connect(manager, &PlaylistManager::PlaylistRenamed, this, &PlaylistListContainer::PlaylistRenamed);
  QObject::connect(manager, &PlaylistManager::CurrentChanged, this, &PlaylistListContainer::CurrentChanged);
  QObject::connect(manager, &PlaylistManager::ActiveChanged, this, &PlaylistListContainer::ActiveChanged);

  QObject::connect(model_, &PlaylistListModel::PlaylistRenamed, manager, &PlaylistManager::Rename);

  QObject::connect(player, &Player::Paused, this, &PlaylistListContainer::ActivePaused);
  QObject::connect(player, &Player::Playing, this, &PlaylistListContainer::ActivePlaying);
  QObject::connect(player, &Player::Stopped, this, &PlaylistListContainer::ActiveStopped);

  // Get all playlists, even ones that are hidden in the UI.
  const QList<PlaylistBackend::Playlist> playlists = app->playlist_backend()->GetAllFavoritePlaylists();
  for (const PlaylistBackend::Playlist &p : playlists) {
    QStandardItem *playlist_item = model_->NewPlaylist(p.name, p.id);
    QStandardItem *parent_folder = model_->FolderByPath(p.ui_path);
    parent_folder->appendRow(playlist_item);
  }

}

void PlaylistListContainer::ReloadSettings() {

  Settings s;
  s.beginGroup(AppearanceSettingsPage::kSettingsGroup);
  int iconsize = s.value(AppearanceSettingsPage::kIconSizeLeftPanelButtons, 22).toInt();
  s.endGroup();

  ui_->new_folder->setIconSize(QSize(iconsize, iconsize));
  ui_->remove->setIconSize(QSize(iconsize, iconsize));
  ui_->save_playlist->setIconSize(QSize(iconsize, iconsize));

}

void PlaylistListContainer::showEvent(QShowEvent *e) {

  // Loading icons is expensive so only do it when the view is first opened
  if (!loaded_icons_) {
    loaded_icons_ = true;

    action_new_folder_->setIcon(IconLoader::Load(QStringLiteral("folder-new")));
    action_remove_->setIcon(IconLoader::Load(QStringLiteral("edit-delete")));
    action_save_playlist_->setIcon(IconLoader::Load(QStringLiteral("document-save")));
#ifndef Q_OS_WIN
    action_copy_to_device_->setIcon(IconLoader::Load(QStringLiteral("device")));
#endif

    model_->SetIcons(IconLoader::Load(QStringLiteral("view-media-playlist")), IconLoader::Load(QStringLiteral("folder")));

    // Apply these icons to items that have already been created.
    RecursivelySetIcons(model_->invisibleRootItem());

  }

  ui_->remove->setEnabled(ui_->tree->ItemsSelected());
  ui_->save_playlist->setEnabled(ui_->tree->ItemsSelected());

  QWidget::showEvent(e);

}

void PlaylistListContainer::RecursivelySetIcons(QStandardItem *parent) const {

  for (int i = 0; i < parent->rowCount(); ++i) {
    QStandardItem *child = parent->child(i);
    switch (child->data(PlaylistListModel::Role_Type).toInt()) {
      case PlaylistListModel::Type_Folder:
        child->setIcon(model_->folder_icon());
        RecursivelySetIcons(child);
        break;

      case PlaylistListModel::Type_Playlist:
        child->setIcon(model_->playlist_icon());
        break;

      default:
        break;
    }
  }

}

void PlaylistListContainer::NewFolderClicked() {

  QString name = QInputDialog::getText(this, tr("New folder"), tr("Enter the name of the folder"));
  if (name.isEmpty()) {
    return;
  }

  name.replace(u'/', u' ');

  model_->invisibleRootItem()->appendRow(model_->NewFolder(name));

}

void PlaylistListContainer::AddPlaylist(const int id, const QString &name, const bool favorite) {

  if (!favorite) {
    return;
  }

  if (model_->PlaylistById(id)) {
    // We know about this playlist already - it was probably one of the open ones that was loaded on startup.
    return;
  }

  const QString &ui_path = app_->playlist_manager()->playlist(id)->ui_path();

  QStandardItem *playlist_item = model_->NewPlaylist(name, id);
  QStandardItem *parent_folder = model_->FolderByPath(ui_path);
  parent_folder->appendRow(playlist_item);

}

void PlaylistListContainer::PlaylistRenamed(const int id, const QString &new_name) {

  QStandardItem *item = model_->PlaylistById(id);
  if (!item) {
    return;
  }

  item->setText(new_name);

}

void PlaylistListContainer::RemovePlaylist(const int id) {

  QStandardItem *item = model_->PlaylistById(id);
  if (item) {
    QStandardItem *parent = item->parent();
    if (!parent) {
      parent = model_->invisibleRootItem();
    }
    parent->removeRow(item->row());
  }

}

void PlaylistListContainer::SavePlaylist() {

  const QModelIndex proxy_idx = ui_->tree->currentIndex();
  if (!proxy_idx.isValid()) return;
  const QModelIndex idx = proxy_->mapToSource(proxy_idx);
  if (!idx.isValid()) return;

  // Is it a playlist?
  if (idx.data(PlaylistListModel::Role_Type).toInt() == PlaylistListModel::Type_Playlist) {
    const int playlist_id = idx.data(PlaylistListModel::Role_PlaylistId).toInt();
    QStandardItem *item = model_->PlaylistById(playlist_id);
    QString playlist_name = item ? item->text() : tr("Playlist");
    app_->playlist_manager()->SaveWithUI(playlist_id, playlist_name);
  }

}

void PlaylistListContainer::PlaylistFavoriteStateChanged(const int id, const bool favorite) {

  if (favorite) {
    const QString &name = app_->playlist_manager()->GetPlaylistName(id);
    AddPlaylist(id, name, favorite);
  }
  else {
    RemovePlaylist(id);
  }

}

void PlaylistListContainer::ActiveChanged(Playlist *new_playlist) {

  const int new_id = new_playlist->id();

  if (new_id != active_playlist_id_) {
    UpdateActiveIcon(active_playlist_id_, QIcon());
  }

  active_playlist_id_ = new_id;

}

void PlaylistListContainer::CurrentChanged(Playlist *new_playlist) {

  if (!new_playlist) {
    return;
  }

  // Focus this playlist in the tree
  QStandardItem *item = model_->PlaylistById(new_playlist->id());
  if (!item) {
    return;
  }

  QModelIndex idx = proxy_->mapFromSource(item->index());
  ui_->tree->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
  ui_->tree->scrollTo(idx);

}

void PlaylistListContainer::PlaylistPathChanged(const int id, const QString &new_path) {

  // Update the path in the database
  app_->playlist_backend()->SetPlaylistUiPath(id, new_path);
  Playlist *playlist = app_->playlist_manager()->playlist(id);
  // Check the playlist exists (if it's not opened it's not in the manager)
  if (playlist) {
    playlist->set_ui_path(new_path);
  }

}

void PlaylistListContainer::ItemsSelectedChanged(const bool selected) {
  ui_->remove->setEnabled(selected);
  ui_->save_playlist->setEnabled(selected);
}

void PlaylistListContainer::ItemDoubleClicked(const QModelIndex &proxy_idx) {

  const QModelIndex idx = proxy_->mapToSource(proxy_idx);
  if (!idx.isValid()) return;

  // Is it a playlist?
  if (idx.data(PlaylistListModel::Role_Type).toInt() == PlaylistListModel::Type_Playlist) {
    app_->playlist_manager()->SetCurrentOrOpen(idx.data(PlaylistListModel::Role_PlaylistId).toInt());
  }

}

void PlaylistListContainer::ItemMimeDataDropped(const QModelIndex &proxy_idx, const QMimeData *q_mimedata) {

  const QModelIndex idx = proxy_->mapToSource(proxy_idx);
  if (!idx.isValid()) return;

  // Drop playlist rows if type is playlist and it's not active, to prevent selfcopy
  int playlis_id = idx.data(PlaylistListModel::Role_PlaylistId).toInt();
  if (idx.data(PlaylistListModel::Role_Type).toInt() == PlaylistListModel::Type_Playlist && playlis_id != app_->playlist_manager()->active_id()) {
    app_->playlist_manager()->playlist(playlis_id)->dropMimeData(q_mimedata, Qt::CopyAction, -1, 0, QModelIndex());
  }

}

void PlaylistListContainer::CopyToDevice() {

#ifndef Q_OS_WIN

  const QModelIndex proxy_idx = ui_->tree->currentIndex();
  if (!proxy_idx.isValid()) return;
  const QModelIndex idx = proxy_->mapToSource(proxy_idx);
  if (!idx.isValid()) return;

  // Is it a playlist?
  if (idx.data(PlaylistListModel::Role_Type).toInt() == PlaylistListModel::Type_Playlist) {
    const int playlist_id = idx.data(PlaylistListModel::Role_PlaylistId).toInt();

    Playlist *playlist = app_->playlist_manager()->playlist(playlist_id);
    if (!playlist) {
      QMessageBox::critical(this, tr("Copy to device"), tr("Playlist must be open first."));
      return;
    }

    QStandardItem *item = model_->PlaylistById(playlist_id);
    QString playlist_name = item ? item->text() : tr("Playlist");

    // Reuse the organize dialog, but set the detail about the playlist name
    if (!organize_dialog_) {
      organize_dialog_ = make_unique<OrganizeDialog>(app_->task_manager(), nullptr, this);
    }
    organize_dialog_->SetDestinationModel(app_->device_manager()->connected_devices_model(), true);
    organize_dialog_->SetCopy(true);
    organize_dialog_->SetPlaylist(playlist_name);
    organize_dialog_->SetSongs(playlist->GetAllSongs());
    organize_dialog_->show();
  }
#endif

}

void PlaylistListContainer::Delete() {

  if (ui_->tree->selectionModel()->selectedRows().count() == 0) return;

  QSet<int> ids;
  QList<QPersistentModelIndex> folders_to_delete;

  const QModelIndexList proxy_indexes = ui_->tree->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex idx = proxy_->mapToSource(proxy_index);

    // Is it a playlist?
    switch (idx.data(PlaylistListModel::Role_Type).toInt()) {
      case PlaylistListModel::Type_Playlist:
        ids << idx.data(PlaylistListModel::Role_PlaylistId).toInt();  // clazy:exclude=reserve-candidates
        break;

      case PlaylistListModel::Type_Folder:
        // Find all the playlists inside.
        RecursivelyFindPlaylists(idx, &ids);
        folders_to_delete << idx;
        break;

      default:
        break;
    }
  }

  // Make sure the user really wants to unfavorite all these playlists.
  if (ids.count() > 1) {
    const int button = QMessageBox::question(this, tr("Remove playlists"), tr("You are about to remove %1 playlists from your favorites, are you sure?").arg(ids.count()), QMessageBox::Yes, QMessageBox::Cancel);

    if (button != QMessageBox::Yes) {
      return;
    }
  }

  // Unfavorite the playlists
  for (const int id : std::as_const(ids)) {
    app_->playlist_manager()->Favorite(id, false);
  }

  // Delete the top-level folders.
  for (const QPersistentModelIndex &idx : std::as_const(folders_to_delete)) {
    if (idx.isValid()) {
      model_->removeRow(idx.row(), idx.parent());
    }
  }

}

void PlaylistListContainer::RecursivelyFindPlaylists(const QModelIndex &parent, QSet<int> *ids) const {

  switch (parent.data(PlaylistListModel::Role_Type).toInt()) {
    case PlaylistListModel::Type_Playlist:
      ids->insert(parent.data(PlaylistListModel::Role_PlaylistId).toInt());
      break;

    case PlaylistListModel::Type_Folder:
      for (int i = 0; i < parent.model()->rowCount(parent); ++i) {
        RecursivelyFindPlaylists(parent.model()->index(i, 0, parent), ids);
      }
      break;
    default:
      break;
  }

}

void PlaylistListContainer::contextMenuEvent(QContextMenuEvent *e) {

  if (!menu_) {
    menu_ = new QMenu(this);
    menu_->addAction(action_new_folder_);
    menu_->addAction(action_remove_);
    menu_->addSeparator();
    menu_->addAction(action_save_playlist_);
#ifndef Q_OS_WIN
    menu_->addSeparator();
    menu_->addAction(action_copy_to_device_);
#endif
  }

  action_remove_->setVisible(ui_->tree->ItemsSelected());
  action_save_playlist_->setVisible(ui_->tree->ItemsSelected());
#ifndef Q_OS_WIN
  action_copy_to_device_->setVisible(ui_->tree->ItemsSelected());
#endif

  menu_->popup(e->globalPos());

}

void PlaylistListContainer::ActivePlaying() {

  if (padded_play_icon_.isNull()) {
    QPixmap pixmap(QStringLiteral(":/pictures/tiny-play.png"));
    QPixmap new_pixmap(QSize(pixmap.height(), pixmap.height()));
    new_pixmap.fill(Qt::transparent);

    QPainter p(&new_pixmap);
    p.drawPixmap((new_pixmap.width() - pixmap.width()) / 2, 0, pixmap.width(), pixmap.height(), pixmap);
    p.end();

    padded_play_icon_.addPixmap(new_pixmap);
  }
  UpdateActiveIcon(active_playlist_id_, padded_play_icon_);

}

void PlaylistListContainer::ActivePaused() {
  UpdateActiveIcon(active_playlist_id_, QIcon(QStringLiteral(":/pictures/tiny-pause.png")));
}

void PlaylistListContainer::ActiveStopped() {
  UpdateActiveIcon(active_playlist_id_, QIcon());
}

void PlaylistListContainer::UpdateActiveIcon(const int id, const QIcon &icon) {

  if (id == -1) {
    return;
  }

  QStandardItem *item = model_->PlaylistById(id);
  if (!item) {
    return;
  }

  if (icon.isNull()) {
    item->setIcon(model_->playlist_icon());
  }
  else {
    item->setIcon(icon);
  }

}
