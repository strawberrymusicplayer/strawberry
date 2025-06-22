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

#include <memory>

#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QAbstractItemView>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QStyleOptionViewItem>
#include <QMimeData>
#include <QAction>
#include <QVariant>
#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QStyle>
#include <QMenu>
#include <QFlags>
#include <QPushButton>
#include <QMessageBox>
#include <QtEvents>

#include "includes/shared_ptr.h"
#include "includes/scoped_ptr.h"
#include "core/iconloader.h"
#include "core/deletefiles.h"
#include "core/mergedproxymodel.h"
#include "core/mimedata.h"
#include "core/musicstorage.h"
#include "utilities/colorutils.h"
#include "organize/organizedialog.h"
#include "organize/organizeerrordialog.h"
#include "collection/collectiondirectorymodel.h"
#include "collection/collectionmodel.h"
#include "collection/collectionitemdelegate.h"
#include "devicelister.h"
#include "connecteddevice.h"
#include "devicemanager.h"
#include "deviceproperties.h"
#include "deviceview.h"

using namespace Qt::Literals::StringLiterals;
using std::make_unique;

const int DeviceItemDelegate::kIconPadding = 6;

DeviceItemDelegate::DeviceItemDelegate(QObject *parent) : CollectionItemDelegate(parent) {}

void DeviceItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const {

  // Is it a device or a collection item?
  if (idx.data(DeviceManager::Role::Role_State).isNull()) {
    CollectionItemDelegate::paint(painter, option, idx);
    return;
  }

  // Draw the background
  const QWidget *widget = option.widget;
  QStyle *style = widget->style() ? widget->style() : QApplication::style();
  style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, widget);

  painter->save();

  // Font for the status line
  QFont status_font(option.font);

#ifdef Q_OS_WIN32
  status_font.setPointSize(status_font.pointSize() - 1);
#else
  status_font.setPointSize(status_font.pointSize() - 2);
#endif

  const int text_height = QFontMetrics(option.font).height() + QFontMetrics(status_font).height();

  QRect line1(option.rect);
  QRect line2(option.rect);
  line1.setTop(line1.top() + (option.rect.height() - text_height) / 2);
  line2.setTop(line1.top() + QFontMetrics(option.font).height());
  line1.setLeft(line1.left() + DeviceManager::kDeviceIconSize + kIconPadding);
  line2.setLeft(line2.left() + DeviceManager::kDeviceIconSize + kIconPadding);

  // Change the color for selected items
  if (option.state & QStyle::State_Selected) {
    painter->setPen(option.palette.color(QPalette::HighlightedText));
  }

  // Draw the icon
  painter->drawPixmap(option.rect.topLeft(), idx.data(Qt::DecorationRole).value<QPixmap>());

  // Draw the first line (device name)
  painter->drawText(line1, Qt::AlignLeft | Qt::AlignTop, idx.data().toString());

  // Draw the second line (status)
  DeviceManager::State state = static_cast<DeviceManager::State>(idx.data(DeviceManager::Role_State).toInt());
  QVariant progress = idx.data(DeviceManager::Role_UpdatingPercentage);
  QString status_text;

  if (progress.isValid()) {
    status_text = tr("Updating %1%...").arg(progress.toInt());
  }
  else {
    switch (state) {
      case DeviceManager::State::Remembered:
        status_text = tr("Not connected");
        break;

      case DeviceManager::State::NotMounted:
        status_text = tr("Not mounted - double click to mount");
        break;

      case DeviceManager::State::NotConnected:
        status_text = tr("Double click to open");
        break;

      case DeviceManager::State::Connected:{
        QVariant song_count = idx.data(DeviceManager::Role_SongCount);
        if (song_count.isValid()) {
          int count = song_count.toInt();
          status_text = tr("%1 song%2").arg(count).arg(count == 1 ? ""_L1 : "s"_L1);
        }
        else {
          status_text = idx.data(DeviceManager::Role_MountPath).toString();
        }
        break;
      }
    }
  }

  if (option.state & QStyle::State_Selected) {
    painter->setPen(option.palette.color(QPalette::HighlightedText));
  }
  else {
    if (Utilities::IsColorDark(option.palette.color(QPalette::Window))) {
      painter->setPen(option.palette.color(QPalette::Midlight).lighter().lighter());
    }
    else {
      painter->setPen(option.palette.color(QPalette::Dark));
    }
  }

  painter->setFont(status_font);
  painter->drawText(line2, Qt::AlignLeft | Qt::AlignTop, status_text);

  painter->restore();

}

DeviceView::DeviceView(QWidget *parent)
    : AutoExpandingTreeView(parent),
      merged_model_(nullptr),
      sort_model_(nullptr),
      properties_dialog_(new DeviceProperties),
      device_menu_(nullptr),
      eject_action_(nullptr),
      forget_action_(nullptr),
      properties_action_(nullptr),
      collection_menu_(nullptr),
      load_action_(nullptr),
      add_to_playlist_action_(nullptr),
      open_in_new_playlist_(nullptr),
      organize_action_(nullptr),
      delete_action_(nullptr) {

  setItemDelegate(new DeviceItemDelegate(this));
  SetExpandOnReset(false);
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setHeaderHidden(true);
  setAllColumnsShowFocus(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
}

DeviceView::~DeviceView() = default;

void DeviceView::Init(const SharedPtr<TaskManager> task_manager,
                      const SharedPtr<TagReaderClient> tagreader_client,
                      const SharedPtr<DeviceManager> device_manager,
                      CollectionDirectoryModel *collection_directory_model) {

  task_manager_ = task_manager;
  tagreader_client_ = tagreader_client;
  device_manager_ = device_manager;

  QObject::connect(&*device_manager_, &DeviceManager::DeviceConnected, this, &DeviceView::DeviceConnected);
  QObject::connect(&*device_manager_, &DeviceManager::DeviceDisconnected, this, &DeviceView::DeviceDisconnected);

  sort_model_ = new QSortFilterProxyModel(this);
  sort_model_->setSourceModel(&*device_manager_);
  sort_model_->setDynamicSortFilter(true);
  sort_model_->setSortCaseSensitivity(Qt::CaseInsensitive);
  sort_model_->sort(0);

  merged_model_ = new MergedProxyModel(this);
  merged_model_->setSourceModel(sort_model_);
  setModel(merged_model_);

  QObject::connect(merged_model_, &MergedProxyModel::SubModelReset, this, &AutoExpandingTreeView::RecursivelyExpandSlot);

  properties_dialog_->Init(device_manager_);

  organize_dialog_ = make_unique<OrganizeDialog>(task_manager, tagreader_client, nullptr, this);
  organize_dialog_->SetDestinationModel(collection_directory_model);

}

void DeviceView::contextMenuEvent(QContextMenuEvent *e) {

  if (!device_menu_) {
    device_menu_ = new QMenu(this);
    collection_menu_ = new QMenu(this);

    // Device menu
    eject_action_ = device_menu_->addAction(IconLoader::Load(u"media-eject"_s), tr("Safely remove device"), this, &DeviceView::Unmount);
    forget_action_ = device_menu_->addAction(IconLoader::Load(u"list-remove"_s), tr("Forget device"), this, &DeviceView::Forget);
    device_menu_->addSeparator();
    properties_action_ = device_menu_->addAction(IconLoader::Load(u"configure"_s), tr("Device properties..."), this, &DeviceView::Properties);

    // Collection menu
    add_to_playlist_action_ = collection_menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Append to current playlist"), this, &DeviceView::AddToPlaylist);
    load_action_ = collection_menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Replace current playlist"), this, &DeviceView::Load);
    open_in_new_playlist_ = collection_menu_->addAction(IconLoader::Load(u"document-new"_s), tr("Open in new playlist"), this, &DeviceView::OpenInNewPlaylist);

    collection_menu_->addSeparator();
    organize_action_ = collection_menu_->addAction(IconLoader::Load(u"edit-copy"_s), tr("Copy to collection..."), this, &DeviceView::Organize);
    delete_action_ = collection_menu_->addAction(IconLoader::Load(u"edit-delete"_s), tr("Delete from device..."), this, &DeviceView::Delete);
  }

  menu_index_ = currentIndex();

  const QModelIndex device_index = MapToDevice(menu_index_);
  const QModelIndex collection_index = MapToCollection(menu_index_);

  if (device_index.isValid()) {
    const bool is_plugged_in = device_manager_->GetLister(device_index);
    const bool is_remembered = device_manager_->GetDatabaseId(device_index) != -1;

    forget_action_->setEnabled(is_remembered);
    eject_action_->setEnabled(is_plugged_in);

    device_menu_->popup(e->globalPos());
  }
  else if (collection_index.isValid()) {
    const QModelIndex parent_device_index = FindParentDevice(menu_index_);

    bool is_filesystem_device = false;
    if (parent_device_index.isValid()) {
      SharedPtr<ConnectedDevice> device = device_manager_->GetConnectedDevice(parent_device_index);
      if (device && !device->LocalPath().isEmpty()) is_filesystem_device = true;
    }

    organize_action_->setEnabled(is_filesystem_device);

    collection_menu_->popup(e->globalPos());
  }

}

QModelIndex DeviceView::MapToDevice(const QModelIndex &merged_model_index) const {

  QModelIndex sort_model_index = merged_model_->mapToSource(merged_model_index);
  if (sort_model_index.model() != sort_model_) return QModelIndex();
  return sort_model_->mapToSource(sort_model_index);

}

QModelIndex DeviceView::FindParentDevice(const QModelIndex &merged_model_index) const {

  QModelIndex idx = merged_model_->FindSourceParent(merged_model_index);
  if (idx.model() != sort_model_) return QModelIndex();
  return sort_model_->mapToSource(idx);

}

QModelIndex DeviceView::MapToCollection(const QModelIndex &merged_model_index) const {

  QModelIndex sort_model_index = merged_model_->mapToSource(merged_model_index);
  if (const QSortFilterProxyModel *sort_model = qobject_cast<const QSortFilterProxyModel*>(sort_model_index.model())) {
    return sort_model->mapToSource(sort_model_index);
  }
  return QModelIndex();

}

void DeviceView::Connect() {
  QModelIndex device_idx = MapToDevice(menu_index_);
  device_manager_->data(device_idx, MusicStorage::Role_StorageForceConnect);
}

void DeviceView::DeviceConnected(const QModelIndex &idx) {

  if (!idx.isValid()) return;

  SharedPtr<ConnectedDevice> device = device_manager_->GetConnectedDevice(idx);
  if (!device) return;

  QModelIndex sort_idx = sort_model_->mapFromSource(idx);
  if (!sort_idx.isValid()) return;

  QSortFilterProxyModel *sort_model = new QSortFilterProxyModel(device->collection_model());
  sort_model->setSourceModel(device->collection_model());
  sort_model->setSortRole(CollectionModel::Role_SortText);
  sort_model->setDynamicSortFilter(true);
  sort_model->sort(0);
  merged_model_->AddSubModel(sort_idx, sort_model);

  expand(menu_index_);

}

void DeviceView::DeviceDisconnected(const QModelIndex &idx) {
  if (!idx.isValid()) return;
  merged_model_->RemoveSubModel(sort_model_->mapFromSource(idx));
}

void DeviceView::Forget() {

  QModelIndex device_idx = MapToDevice(menu_index_);
  QString unique_id = device_manager_->data(device_idx, DeviceManager::Role_UniqueId).toString();
  if (device_manager_->GetLister(device_idx) && device_manager_->GetLister(device_idx)->AskForScan(unique_id)) {
    ScopedPtr<QMessageBox> dialog(new QMessageBox(
        QMessageBox::Question, tr("Forget device"),
        tr("Forgetting a device will remove it from this list and Strawberry will have to rescan all the songs again next time you connect it."),
        QMessageBox::Cancel, this));
    QPushButton *forget = dialog->addButton(tr("Forget device"), QMessageBox::DestructiveRole);
    dialog->exec();

    if (dialog->clickedButton() != forget) return;
  }

  device_manager_->Forget(device_idx);

}

void DeviceView::Properties() {
  properties_dialog_->ShowDevice(MapToDevice(menu_index_));
}

void DeviceView::mouseDoubleClickEvent(QMouseEvent *e) {

  AutoExpandingTreeView::mouseDoubleClickEvent(e);

  QModelIndex merged_index = indexAt(e->pos());
  QModelIndex device_index = MapToDevice(merged_index);
  if (device_index.isValid()) {
    if (!device_manager_->GetConnectedDevice(device_index)) {
      menu_index_ = merged_index;
      Connect();
    }
  }

}

SongList DeviceView::GetSelectedSongs() const {

  const QModelIndexList selected_merged_indexes = selectionModel()->selectedRows();
  SongList songs;
  for (const QModelIndex &merged_index : selected_merged_indexes) {
    QModelIndex collection_index = MapToCollection(merged_index);
    if (!collection_index.isValid()) continue;

    const CollectionModel *collection = qobject_cast<const CollectionModel*>(collection_index.model());
    if (!collection) continue;

    songs << collection->GetChildSongs(collection_index);
  }
  return songs;

}

void DeviceView::Load() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->clear_first_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void DeviceView::AddToPlaylist() {
  Q_EMIT AddToPlaylistSignal(model()->mimeData(selectedIndexes()));
}

void DeviceView::OpenInNewPlaylist() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->open_in_new_playlist_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void DeviceView::Delete() {

  QModelIndexList selected_indexes = selectedIndexes();

  if (selected_indexes.isEmpty()) return;

  // Take the device of the first selected item
  QModelIndex device_index = FindParentDevice(selected_indexes[0]);
  if (!device_index.isValid()) return;

  if (QMessageBox::question(this, tr("Delete files"), tr("These files will be deleted from the device, are you sure you want to continue?"), QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes) {
    return;
  }

  SharedPtr<MusicStorage> storage = device_index.data(MusicStorage::Role_Storage).value<SharedPtr<MusicStorage>>();

  DeleteFiles *delete_files = new DeleteFiles(task_manager_, storage, false);
  QObject::connect(delete_files, &DeleteFiles::Finished, this, &DeviceView::DeleteFinished);
  delete_files->Start(GetSelectedSongs());

}

void DeviceView::Organize() {

  const SongList songs = GetSelectedSongs();
  QStringList filenames;
  filenames.reserve(songs.count());
  for (const Song &song : songs) {
    filenames << song.url().toLocalFile();
  }

  organize_dialog_->SetCopy(true);
  organize_dialog_->SetFilenames(filenames);
  organize_dialog_->show();

}

void DeviceView::Unmount() {
  QModelIndex device_idx = MapToDevice(menu_index_);
  device_manager_->Unmount(device_idx);
}

void DeviceView::DeleteFinished(const SongList &songs_with_errors) {

  if (songs_with_errors.isEmpty()) return;

  OrganizeErrorDialog *dialog = new OrganizeErrorDialog(this);
  dialog->Show(OrganizeErrorDialog::OperationType::Delete, songs_with_errors);
  // It deletes itself when the user closes it

}

bool DeviceView::CanRecursivelyExpand(const QModelIndex &idx) const {
  // Never expand devices
  return idx.parent().isValid();
}
