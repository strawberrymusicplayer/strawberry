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

#include <QtGlobal>
#include <QWidget>
#include <QAbstractItemView>
#include <QTreeView>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QMimeData>
#include <QSet>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QPixmap>
#include <QPainter>
#include <QRect>
#include <QFont>
#include <QFontMetrics>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QSettings>
#include <QtEvents>

#include "core/application.h"
#include "core/iconloader.h"
#include "core/mimedata.h"
#include "core/musicstorage.h"
#include "core/deletefiles.h"
#include "core/settings.h"
#include "utilities/filemanagerutils.h"
#include "collection.h"
#include "collectionbackend.h"
#include "collectiondirectorymodel.h"
#include "collectionfilterwidget.h"
#include "collectionitem.h"
#include "collectionitemdelegate.h"
#include "collectionmodel.h"
#include "collectionview.h"
#ifndef Q_OS_WIN
#  include "device/devicemanager.h"
#  include "device/devicestatefiltermodel.h"
#endif
#include "dialogs/edittagdialog.h"
#include "dialogs/deleteconfirmationdialog.h"
#include "organize/organizedialog.h"
#include "organize/organizeerrordialog.h"
#include "settings/collectionsettingspage.h"

using std::make_unique;

CollectionView::CollectionView(QWidget *parent)
    : AutoExpandingTreeView(parent),
      app_(nullptr),
      filter_(nullptr),
      total_song_count_(-1),
      total_artist_count_(-1),
      total_album_count_(-1),
      nomusic_(QStringLiteral(":/pictures/nomusic.png")),
      context_menu_(nullptr),
      action_load_(nullptr),
      action_add_to_playlist_(nullptr),
      action_add_to_playlist_enqueue_(nullptr),
      action_add_to_playlist_enqueue_next_(nullptr),
      action_open_in_new_playlist_(nullptr),
      action_organize_(nullptr),
      action_search_for_this_(nullptr),
#ifndef Q_OS_WIN
      action_copy_to_device_(nullptr),
#endif
      action_edit_track_(nullptr),
      action_edit_tracks_(nullptr),
      action_rescan_songs_(nullptr),
      action_show_in_browser_(nullptr),
      action_show_in_various_(nullptr),
      action_no_show_in_various_(nullptr),
      action_delete_files_(nullptr),
      is_in_keyboard_search_(false),
      delete_files_(false) {

  setObjectName(QLatin1String(metaObject()->className()));

  setItemDelegate(new CollectionItemDelegate(this));
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setHeaderHidden(true);
  setAllColumnsShowFocus(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::ExtendedSelection);

  setStyleSheet(QStringLiteral("QTreeView::item{padding-top:1px;}"));

}

CollectionView::~CollectionView() = default;

void CollectionView::SaveFocus() {

  const QModelIndex current = currentIndex();
  const QVariant role_type = model()->data(current, CollectionModel::Role_Type);
  if (!role_type.isValid()) {
    return;
  }

  const CollectionItem::Type item_type = role_type.value<CollectionItem::Type>();
  if (item_type != CollectionItem::Type::Song && item_type != CollectionItem::Type::Container && item_type != CollectionItem::Type::Divider) {
    return;
  }

  last_selected_path_.clear();
  last_selected_song_ = Song();
  last_selected_container_ = QString();

  switch (item_type) {
    case CollectionItem::Type::Song:{
      QModelIndex index = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(current);
      SongList songs = app_->collection_model()->GetChildSongs(index);
      if (!songs.isEmpty()) {
        last_selected_song_ = songs.last();
      }
      break;
    }

    case CollectionItem::Type::Container:
    case CollectionItem::Type::Divider:{
      QString text = model()->data(current, CollectionModel::Role_SortText).toString();
      last_selected_container_ = text;
      break;
    }

    default:
      return;
  }

  SaveContainerPath(current);

}

void CollectionView::SaveContainerPath(const QModelIndex &child) {

  const QModelIndex current = model()->parent(child);
  const QVariant role_type = model()->data(current, CollectionModel::Role_Type);
  if (!role_type.isValid()) {
    return;
  }

  const CollectionItem::Type item_type = role_type.value<CollectionItem::Type>();
  if (item_type != CollectionItem::Type::Container && item_type != CollectionItem::Type::Divider) {
    return;
  }

  QString text = model()->data(current, CollectionModel::Role_SortText).toString();
  last_selected_path_ << text;
  SaveContainerPath(current);

}

void CollectionView::RestoreFocus() {

  if (last_selected_container_.isEmpty() && last_selected_song_.url().isEmpty()) {
    return;
  }
  RestoreLevelFocus();

}

bool CollectionView::RestoreLevelFocus(const QModelIndex &parent) {

  if (model()->canFetchMore(parent)) {
    model()->fetchMore(parent);
  }
  const int rows = model()->rowCount(parent);
  for (int i = 0; i < rows; i++) {
    QModelIndex current = model()->index(i, 0, parent);
    const QVariant role_type = model()->data(current, CollectionModel::Role_Type);
    if (!role_type.isValid()) return false;
    const CollectionItem::Type item_type = role_type.value<CollectionItem::Type>();
    switch (item_type) {
      case CollectionItem::Type::Root:
      case CollectionItem::Type::LoadingIndicator:
        break;
      case CollectionItem::Type::Song:
        if (!last_selected_song_.url().isEmpty()) {
          QModelIndex index = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(current);
          const SongList songs = app_->collection_model()->GetChildSongs(index);
          if (std::any_of(songs.begin(), songs.end(), [this](const Song &song) { return song == last_selected_song_; })) {
            setCurrentIndex(current);
            return true;
          }
        }
        break;

      case CollectionItem::Type::Container:
      case CollectionItem::Type::Divider:{
        QString text = model()->data(current, CollectionModel::Role_SortText).toString();
        if (!last_selected_container_.isEmpty() && last_selected_container_ == text) {
          expand(current);
          setCurrentIndex(current);
          return true;
        }
        else if (last_selected_path_.contains(text)) {
          expand(current);
          // If a selected container or song were not found, we've got into a wrong subtree (happens with "unknown" all the time)
          if (!RestoreLevelFocus(current)) {
            collapse(current);
          }
          else {
            return true;
          }
        }
        break;
      }
    }
  }
  return false;

}

void CollectionView::ReloadSettings() {

  Settings settings;
  settings.beginGroup(CollectionSettingsPage::kSettingsGroup);
  SetAutoOpen(settings.value("auto_open", false).toBool());
  delete_files_ = settings.value("delete_files", false).toBool();
  settings.endGroup();

}

void CollectionView::SetApplication(Application *app) {

  app_ = app;

  ReloadSettings();

}

void CollectionView::SetFilter(CollectionFilterWidget *filter) { filter_ = filter; }

void CollectionView::TotalSongCountUpdated(const int count) {

  int old = total_song_count_;
  total_song_count_ = count;
  if (old != total_song_count_) update();

  if (total_song_count_ == 0) {
    setCursor(Qt::PointingHandCursor);
  }
  else {
    unsetCursor();
  }

  Q_EMIT TotalSongCountUpdated_();

}

void CollectionView::TotalArtistCountUpdated(const int count) {

  int old = total_artist_count_;
  total_artist_count_ = count;
  if (old != total_artist_count_) update();

  if (total_artist_count_ == 0) {
    setCursor(Qt::PointingHandCursor);
  }
  else {
    unsetCursor();
  }

  Q_EMIT TotalArtistCountUpdated_();

}

void CollectionView::TotalAlbumCountUpdated(const int count) {

  int old = total_album_count_;
  total_album_count_ = count;
  if (old != total_album_count_) update();

  if (total_album_count_ == 0) {
    setCursor(Qt::PointingHandCursor);
  }
  else {
    unsetCursor();
  }

  Q_EMIT TotalAlbumCountUpdated_();

}

void CollectionView::paintEvent(QPaintEvent *event) {

  if (total_song_count_ == 0) {
    QPainter p(viewport());
    QRect rect(viewport()->rect());

    // Draw the confused strawberry
    QRect image_rect((rect.width() - nomusic_.width()) / 2, 50, nomusic_.width(), nomusic_.height());
    p.drawPixmap(image_rect, nomusic_);

    // Draw the title text
    QFont bold_font;
    bold_font.setBold(true);
    p.setFont(bold_font);

    QFontMetrics metrics(bold_font);

    QRect title_rect(0, image_rect.bottom() + 20, rect.width(), metrics.height());
    p.drawText(title_rect, Qt::AlignHCenter, tr("Your collection is empty!"));

    // Draw the other text
    p.setFont(QFont());

    QRect text_rect(0, title_rect.bottom() + 5, rect.width(), metrics.height());
    p.drawText(text_rect, Qt::AlignHCenter, tr("Click here to add some music"));
  }
  else {
    QTreeView::paintEvent(event);
  }

}

void CollectionView::mouseReleaseEvent(QMouseEvent *e) {

  QTreeView::mouseReleaseEvent(e);

  if (total_song_count_ == 0) {
    Q_EMIT ShowConfigDialog();
  }

}

void CollectionView::keyPressEvent(QKeyEvent *e) {

  switch (e->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
      if (currentIndex().isValid()) {
        AddToPlaylist();
      }
      e->accept();
      break;
    default:
      break;
  }

  AutoExpandingTreeView::keyPressEvent(e);

}

void CollectionView::contextMenuEvent(QContextMenuEvent *e) {

  if (!context_menu_) {
    context_menu_ = new QMenu(this);
    action_add_to_playlist_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("media-playback-start")), tr("Append to current playlist"), this, &CollectionView::AddToPlaylist);
    action_load_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("media-playback-start")), tr("Replace current playlist"), this, &CollectionView::Load);
    action_open_in_new_playlist_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("document-new")), tr("Open in new playlist"), this, &CollectionView::OpenInNewPlaylist);

    context_menu_->addSeparator();
    action_add_to_playlist_enqueue_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("go-next")), tr("Queue track"), this, &CollectionView::AddToPlaylistEnqueue);
    action_add_to_playlist_enqueue_next_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("go-next")), tr("Queue to play next"), this, &CollectionView::AddToPlaylistEnqueueNext);

    context_menu_->addSeparator();

    action_search_for_this_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("edit-find")), tr("Search for this"), this, &CollectionView::SearchForThis);

    context_menu_->addSeparator();
    action_organize_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("edit-copy")), tr("Organize files..."), this, &CollectionView::Organize);
#ifndef Q_OS_WIN
    action_copy_to_device_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("device")), tr("Copy to device..."), this, &CollectionView::CopyToDevice);
#endif
    action_delete_files_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("edit-delete")), tr("Delete from disk..."), this, &CollectionView::Delete);

    context_menu_->addSeparator();
    action_edit_track_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("edit-rename")), tr("Edit track information..."), this, &CollectionView::EditTracks);
    action_edit_tracks_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("edit-rename")), tr("Edit tracks information..."), this, &CollectionView::EditTracks);
    action_show_in_browser_ = context_menu_->addAction(IconLoader::Load(QStringLiteral("document-open-folder")), tr("Show in file browser..."), this, &CollectionView::ShowInBrowser);

    context_menu_->addSeparator();

    action_rescan_songs_ = context_menu_->addAction(tr("Rescan song(s)"), this, &CollectionView::RescanSongs);

    context_menu_->addSeparator();
    action_show_in_various_ = context_menu_->addAction(tr("Show in various artists"), this, &CollectionView::ShowInVarious);
    action_no_show_in_various_ = context_menu_->addAction(tr("Don't show in various artists"), this, &CollectionView::NoShowInVarious);

    context_menu_->addSeparator();

    context_menu_->addMenu(filter_->menu());

#ifndef Q_OS_WIN
    action_copy_to_device_->setDisabled(app_->device_manager()->connected_devices_model()->rowCount() == 0);
    QObject::connect(app_->device_manager()->connected_devices_model(), &DeviceStateFilterModel::IsEmptyChanged, action_copy_to_device_, &QAction::setDisabled);
#endif

  }

  context_menu_index_ = indexAt(e->pos());
  if (!context_menu_index_.isValid()) return;

  context_menu_index_ = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(context_menu_index_);

  const QModelIndexList selected_indexes = qobject_cast<QSortFilterProxyModel*>(model())->mapSelectionToSource(selectionModel()->selection()).indexes();

  int regular_elements = 0;
  int regular_editable = 0;

  for (const QModelIndex &idx : selected_indexes) {
    ++regular_elements;
    if (app_->collection_model()->data(idx, CollectionModel::Role_Editable).toBool()) {
      ++regular_editable;
    }
  }

  const int songs_selected = regular_elements;

  // in all modes
  action_load_->setEnabled(songs_selected > 0);
  action_add_to_playlist_->setEnabled(songs_selected > 0);
  action_open_in_new_playlist_->setEnabled(songs_selected > 0);
  action_add_to_playlist_enqueue_->setEnabled(songs_selected > 0);

  // if neither edit_track not edit_tracks are available, we show disabled edit_track element
  action_edit_track_->setVisible(regular_editable == 1);
  action_edit_track_->setEnabled(regular_editable == 1);
  action_edit_tracks_->setVisible(regular_editable > 1);
  action_edit_tracks_->setEnabled(regular_editable > 1);

  action_rescan_songs_->setVisible(regular_editable > 0);
  action_rescan_songs_->setEnabled(regular_editable > 0);

  action_organize_->setVisible(regular_elements == regular_editable);
#ifndef Q_OS_WIN
  action_copy_to_device_->setVisible(regular_elements == regular_editable);
#endif

  action_delete_files_->setVisible(delete_files_);

  action_show_in_various_->setVisible(songs_selected > 0);
  action_no_show_in_various_->setVisible(songs_selected > 0);

  // only when all selected items are editable
  action_organize_->setEnabled(regular_elements == regular_editable);
#ifndef Q_OS_WIN
  action_copy_to_device_->setEnabled(regular_elements == regular_editable);
#endif

  action_delete_files_->setEnabled(delete_files_);

  context_menu_->popup(e->globalPos());

}

void CollectionView::ShowInVarious() { SetShowInVarious(true); }

void CollectionView::NoShowInVarious() { SetShowInVarious(false); }

void CollectionView::SetShowInVarious(const bool on) {

  if (!context_menu_index_.isValid()) return;

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
  if (on && albums.keys().count() == 1) {
    const QStringList albums_list = albums.keys();
    const QString album = albums_list.first();
    const SongList all_of_album = app_->collection_backend()->GetSongsByAlbum(album);
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
    app_->collection_backend()->ForceCompilation(album, albums.values(album), on);
  }

}

void CollectionView::Load() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->clear_first_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionView::AddToPlaylist() {

  Q_EMIT AddToPlaylistSignal(model()->mimeData(selectedIndexes()));

}

void CollectionView::AddToPlaylistEnqueue() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_now_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionView::AddToPlaylistEnqueueNext() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_next_now_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionView::OpenInNewPlaylist() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->open_in_new_playlist_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionView::SearchForThis() {

  const QModelIndex current = currentIndex();
  const QVariant role_type = model()->data(current, CollectionModel::Role_Type);
  if (!role_type.isValid()) {
    return;
  }

  const CollectionItem::Type item_type = role_type.value<CollectionItem::Type>();
  if (item_type != CollectionItem::Type::Song && item_type != CollectionItem::Type::Container && item_type != CollectionItem::Type::Divider) {
    return;
  }
  QString search;
  QModelIndex index = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(current);

  switch (item_type) {
    case CollectionItem::Type::Song:{
      SongList songs = app_->collection_model()->GetChildSongs(index);
      if (!songs.isEmpty()) {
        last_selected_song_ = songs.last();
      }
      search = QStringLiteral("title:\"%1\"").arg(last_selected_song_.title());
      break;
    }

    case CollectionItem::Type::Divider:{
      break;
    }

    case CollectionItem::Type::Container:{
      CollectionItem *item = app_->collection_model()->IndexToItem(index);
      const CollectionModel::GroupBy group_by = app_->collection_model()->GetGroupBy()[item->container_level];
      while (!item->children.isEmpty()) {
        item = item->children.constFirst();
      }

      switch (group_by) {
        case CollectionModel::GroupBy::AlbumArtist:
          search = QStringLiteral("albumartist:\"%1\"").arg(item->metadata.effective_albumartist());
          break;
        case CollectionModel::GroupBy::Artist:
          search = QStringLiteral("artist:\"%1\"").arg(item->metadata.artist());
          break;
        case CollectionModel::GroupBy::Album:
        case CollectionModel::GroupBy::AlbumDisc:
          search = QStringLiteral("album:\"%1\"").arg(item->metadata.album());
          break;
        case CollectionModel::GroupBy::YearAlbum:
        case CollectionModel::GroupBy::YearAlbumDisc:
          search = QStringLiteral("year:%1 album:\"%2\"").arg(item->metadata.year()).arg(item->metadata.album());
          break;
        case CollectionModel::GroupBy::OriginalYearAlbum:
        case CollectionModel::GroupBy::OriginalYearAlbumDisc:
          search = QStringLiteral("year:%1 album:\"%2\"").arg(item->metadata.effective_originalyear()).arg(item->metadata.album());
          break;
        case CollectionModel::GroupBy::Year:
          search = QStringLiteral("year:%1").arg(item->metadata.year());
          break;
        case CollectionModel::GroupBy::OriginalYear:
          search = QStringLiteral("year:%1").arg(item->metadata.effective_originalyear());
          break;
        case CollectionModel::GroupBy::Genre:
          search = QStringLiteral("genre:\"%1\"").arg(item->metadata.genre());
          break;
        case CollectionModel::GroupBy::Composer:
          search = QStringLiteral("composer:\"%1\"").arg(item->metadata.composer());
          break;
        case CollectionModel::GroupBy::Performer:
          search = QStringLiteral("performer:\"%1\"").arg(item->metadata.performer());
          break;
        case CollectionModel::GroupBy::Grouping:
          search = QStringLiteral("grouping:\"%1\"").arg(item->metadata.grouping());
          break;
        case CollectionModel::GroupBy::Samplerate:
          search = QStringLiteral("samplerate:%1").arg(item->metadata.samplerate());
          break;
        case CollectionModel::GroupBy::Bitdepth:
          search = QStringLiteral("bitdepth:%1").arg(item->metadata.bitdepth());
          break;
        case CollectionModel::GroupBy::Bitrate:
          search = QStringLiteral("bitrate:%1").arg(item->metadata.bitrate());
          break;
        default:
          search = model()->data(current, Qt::DisplayRole).toString();
      }
      break;
    }

    default:
      return;
  }

  filter_->ShowInCollection(search);

}

void CollectionView::keyboardSearch(const QString &search) {

  is_in_keyboard_search_ = true;
  QTreeView::keyboardSearch(search);
  is_in_keyboard_search_ = false;

}

void CollectionView::scrollTo(const QModelIndex &idx, ScrollHint hint) {

  if (is_in_keyboard_search_) {
    QTreeView::scrollTo(idx, QAbstractItemView::PositionAtTop);
  }
  else {
    QTreeView::scrollTo(idx, hint);
  }

}

SongList CollectionView::GetSelectedSongs() const {

  QModelIndexList selected_indexes = qobject_cast<QSortFilterProxyModel*>(model())->mapSelectionToSource(selectionModel()->selection()).indexes();
  return app_->collection_model()->GetChildSongs(selected_indexes);

}

void CollectionView::Organize() {

  if (!organize_dialog_) {
    organize_dialog_ = make_unique<OrganizeDialog>(app_->task_manager(), app_->collection_backend(), this);
  }

  organize_dialog_->SetDestinationModel(app_->collection_model()->directory_model());
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
    edit_tag_dialog_ = make_unique<EditTagDialog>(app_, this);
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

  app_->collection()->Rescan(GetSelectedSongs());

}

void CollectionView::CopyToDevice() {

#ifndef Q_OS_WIN
  if (!organize_dialog_) {
    organize_dialog_ = make_unique<OrganizeDialog>(app_->task_manager(), nullptr, this);
  }

  organize_dialog_->SetDestinationModel(app_->device_manager()->connected_devices_model(), true);
  organize_dialog_->SetCopy(true);
  organize_dialog_->SetSongs(GetSelectedSongs());
  organize_dialog_->show();
#endif

}

void CollectionView::FilterReturnPressed() {

  if (!currentIndex().isValid()) {
    // Pick the first thing that isn't a divider
    for (int row = 0; row < model()->rowCount(); ++row) {
      const QModelIndex idx = model()->index(row, 0);
      const QVariant role_type = idx.data(CollectionModel::Role_Type);
      if (!role_type.isValid()) continue;
      const CollectionItem::Type item_type = role_type.value<CollectionItem::Type>();
      if (item_type != CollectionItem::Type::Divider) {
        setCurrentIndex(idx);
        break;
      }
    }
  }

  if (!currentIndex().isValid()) return;

  Q_EMIT doubleClicked(currentIndex());
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

int CollectionView::TotalSongs() const {
  return total_song_count_;
}
int CollectionView::TotalArtists() const {
  return total_artist_count_;
}
int CollectionView::TotalAlbums() const {
  return total_album_count_;
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
  SharedPtr<MusicStorage> storage = app_->collection_model()->directory_model()->index(0, 0).data(MusicStorage::Role_Storage).value<SharedPtr<MusicStorage>>();

  DeleteFiles *delete_files = new DeleteFiles(app_->task_manager(), storage, true);
  QObject::connect(delete_files, &DeleteFiles::Finished, this, &CollectionView::DeleteFilesFinished);
  delete_files->Start(songs);

}

void CollectionView::DeleteFilesFinished(const SongList &songs_with_errors) {

  if (songs_with_errors.isEmpty()) return;

  OrganizeErrorDialog *dialog = new OrganizeErrorDialog(this);
  dialog->Show(OrganizeErrorDialog::OperationType::Delete, songs_with_errors);
  // It deletes itself when the user closes it

}
