/*
 * Strawberry Music Player
 * This code was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <qcoreevent.h>

#include <QtGlobal>
#include <QTreeView>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QAbstractItemView>
#include <QStyleOptionViewItem>
#include <QAbstractScrollArea>
#include <QMimeData>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QLocale>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QRect>
#include <QSize>
#include <QToolTip>
#include <QWhatsThis>

#include "core/application.h"
#include "core/iconloader.h"
#include "core/mimedata.h"
#include "core/utilities.h"
#include "collection/collectiondirectorymodel.h"
#include "collection/collectionmodel.h"
#include "collection/collectionitem.h"
#ifndef Q_OS_WIN
#  include "device/devicemanager.h"
#  include "device/devicestatefiltermodel.h"
#endif
#include "dialogs/edittagdialog.h"
#include "organize/organizedialog.h"

#include "contextalbumsmodel.h"
#include "contextalbumsview.h"

ContextItemDelegate::ContextItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

bool ContextItemDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &idx) {

  return true;

  Q_UNUSED(option);

  if (!event || !view) return false;

  QString text = displayText(idx.data(), QLocale::system());

  if (text.isEmpty() || !event) return false;

  switch (event->type()) {
    case QEvent::ToolTip: {

      QSize real_text = sizeHint(option, idx);
      QRect displayed_text = view->visualRect(idx);
      bool is_elided = displayed_text.width() < real_text.width();

      if (is_elided) {
        QToolTip::showText(event->globalPos(), text, view);
      }
      else if (idx.data(Qt::ToolTipRole).isValid()) {
        // If the item has a tooltip text, display it
        QString tooltip_text = idx.data(Qt::ToolTipRole).toString();
        QToolTip::showText(event->globalPos(), tooltip_text, view);
      }
      else {
        // in case that another text was previously displayed
        QToolTip::hideText();
      }
      return true;
    }

    case QEvent::QueryWhatsThis:
      return true;

    case QEvent::WhatsThis:
      QWhatsThis::showText(event->globalPos(), text, view);
      return true;

    default:
      break;
  }
  return false;

}

ContextAlbumsView::ContextAlbumsView(QWidget *parent)
    : AutoExpandingTreeView(parent),
      app_(nullptr),
      context_menu_(nullptr),
      load_(nullptr),
      add_to_playlist_(nullptr),
      add_to_playlist_enqueue_(nullptr),
      open_in_new_playlist_(nullptr),
      organize_(nullptr),
#ifndef Q_OS_WIN
      copy_to_device_(nullptr),
#endif
      edit_track_(nullptr),
      edit_tracks_(nullptr),
      show_in_browser_(nullptr),
      is_in_keyboard_search_(false),
      model_(nullptr) {

  setStyleSheet("border: none;");

  setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  setItemDelegate(new ContextItemDelegate(this));
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setHeaderHidden(true);
  setAllColumnsShowFocus(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  SetAddOnDoubleClick(false);

}

ContextAlbumsView::~ContextAlbumsView() = default;

void ContextAlbumsView::SaveFocus() {

  QModelIndex current = currentIndex();
  QVariant type = model()->data(current, ContextAlbumsModel::Role_Type);
  if (!type.isValid() || !(type.toInt() == CollectionItem::Type_Song || type.toInt() == CollectionItem::Type_Container || type.toInt() == CollectionItem::Type_Divider)) {
    return;
  }

  last_selected_path_.clear();
  last_selected_song_ = Song();
  last_selected_container_ = QString();

  switch (type.toInt()) {
    case CollectionItem::Type_Song: {
      QModelIndex index = current;
      SongList songs = model_->GetChildSongs(index);
      if (!songs.isEmpty()) {
        last_selected_song_ = songs.last();
      }
      break;
    }

    case CollectionItem::Type_Container:
    case CollectionItem::Type_Divider: {
      break;
    }

    default:
      return;
  }

  SaveContainerPath(current);

}

void ContextAlbumsView::SaveContainerPath(const QModelIndex &child) {

  QModelIndex current = model()->parent(child);
  QVariant type = model()->data(current, ContextAlbumsModel::Role_Type);
  if (!type.isValid() || !(type.toInt() == CollectionItem::Type_Container || type.toInt() == CollectionItem::Type_Divider)) {
    return;
  }

  QString text = model()->data(current, ContextAlbumsModel::Role_SortText).toString();
  last_selected_path_ << text;
  SaveContainerPath(current);

}

void ContextAlbumsView::RestoreFocus() {

  if (last_selected_container_.isEmpty() && last_selected_song_.url().isEmpty()) {
    return;
  }
  RestoreLevelFocus();

}

bool ContextAlbumsView::RestoreLevelFocus(const QModelIndex &parent) {

  if (model()->canFetchMore(parent)) {
    model()->fetchMore(parent);
  }
  int rows = model()->rowCount(parent);
  for (int i = 0; i < rows; i++) {
    QModelIndex current = model()->index(i, 0, parent);
    QVariant type = model()->data(current, ContextAlbumsModel::Role_Type);
    switch (type.toInt()) {
      case CollectionItem::Type_Song:
        if (!last_selected_song_.url().isEmpty()) {
          QModelIndex index = qobject_cast<QSortFilterProxyModel *>(model())->mapToSource(current);
          const SongList songs = model_->GetChildSongs(index);
          if (std::any_of(songs.begin(), songs.end(), [this](const Song &song) { return song == last_selected_song_; })) {
            setCurrentIndex(current);
            return true;
          }
        }
        break;
    }
  }
  return false;

}

void ContextAlbumsView::Init(Application *app) {

  app_ = app;

  model_ = new ContextAlbumsModel(app_->collection_backend(), app_, this);
  model_->Reset();

  setModel(model_);

  QObject::connect(model_, &ContextAlbumsModel::modelAboutToBeReset, this, &ContextAlbumsView::SaveFocus);
  QObject::connect(model_, &ContextAlbumsModel::modelReset, this, &ContextAlbumsView::RestoreFocus);

}

void ContextAlbumsView::paintEvent(QPaintEvent *event) {
  QTreeView::paintEvent(event);

}

void ContextAlbumsView::mouseReleaseEvent(QMouseEvent *e) {
  QTreeView::mouseReleaseEvent(e);

}

void ContextAlbumsView::contextMenuEvent(QContextMenuEvent *e) {

  if (!context_menu_) {
    context_menu_ = new QMenu(this);

    add_to_playlist_ = context_menu_->addAction(IconLoader::Load("media-playback-start"), tr("Append to current playlist"), this, &ContextAlbumsView::AddToPlaylist);
    load_ = context_menu_->addAction(IconLoader::Load("media-playback-start"), tr("Replace current playlist"), this, &ContextAlbumsView::Load);
    open_in_new_playlist_ = context_menu_->addAction(IconLoader::Load("document-new"), tr("Open in new playlist"), this, &ContextAlbumsView::OpenInNewPlaylist);

    context_menu_->addSeparator();
    add_to_playlist_enqueue_ = context_menu_->addAction(IconLoader::Load("go-next"), tr("Queue track"), this, &ContextAlbumsView::AddToPlaylistEnqueue);

    context_menu_->addSeparator();
    organize_ = context_menu_->addAction(IconLoader::Load("edit-copy"), tr("Organize files..."), this, &ContextAlbumsView::Organize);
#ifndef Q_OS_WIN
    copy_to_device_ = context_menu_->addAction(IconLoader::Load("device"), tr("Copy to device..."), this, &ContextAlbumsView::CopyToDevice);
#endif

    context_menu_->addSeparator();
    edit_track_ = context_menu_->addAction(IconLoader::Load("edit-rename"), tr("Edit track information..."), this, &ContextAlbumsView::EditTracks);
    edit_tracks_ = context_menu_->addAction(IconLoader::Load("edit-rename"), tr("Edit tracks information..."), this, &ContextAlbumsView::EditTracks);
    show_in_browser_ = context_menu_->addAction(IconLoader::Load("document-open-folder"), tr("Show in file browser..."), this, &ContextAlbumsView::ShowInBrowser);

    context_menu_->addSeparator();

#ifndef Q_OS_WIN
    copy_to_device_->setDisabled(app_->device_manager()->connected_devices_model()->rowCount() == 0);
    QObject::connect(app_->device_manager()->connected_devices_model(), &DeviceStateFilterModel::IsEmptyChanged, copy_to_device_, &QAction::setDisabled);
#endif
  }

  context_menu_index_ = indexAt(e->pos());
  if (!context_menu_index_.isValid()) return;
  QModelIndexList selected_indexes = selectionModel()->selectedRows();

  int regular_elements = 0;
  int regular_editable = 0;

  for (const QModelIndex &idx : selected_indexes) {
    regular_elements++;
    if (model_->data(idx, ContextAlbumsModel::Role_Editable).toBool()) {
      regular_editable++;
    }
  }

  // TODO: check if custom plugin actions should be enabled / visible
  const int songs_selected = regular_elements;
  const bool regular_elements_only = songs_selected == regular_elements && regular_elements > 0;

  // in all modes
  load_->setEnabled(songs_selected > 0);
  add_to_playlist_->setEnabled(songs_selected > 0);
  open_in_new_playlist_->setEnabled(songs_selected > 0);
  add_to_playlist_enqueue_->setEnabled(songs_selected > 0);

  // if neither edit_track not edit_tracks are available, we show disabled edit_track element
  edit_track_->setVisible(regular_editable <= 1);
  edit_track_->setEnabled(regular_editable == 1);

  organize_->setVisible(regular_elements_only);
#ifndef Q_OS_WIN
  copy_to_device_->setVisible(regular_elements_only);
#endif

  // only when all selected items are editable
  organize_->setEnabled(regular_elements == regular_editable);
#ifndef Q_OS_WIN
  copy_to_device_->setEnabled(regular_elements == regular_editable);
#endif

  context_menu_->popup(e->globalPos());

}

void ContextAlbumsView::Load() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData *>(q_mimedata)) {
    mimedata->clear_first_ = true;
  }
  emit AddToPlaylistSignal(q_mimedata);

}

void ContextAlbumsView::AddToPlaylist() {

  emit AddToPlaylistSignal(model()->mimeData(selectedIndexes()));

}

void ContextAlbumsView::AddToPlaylistEnqueue() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData *>(q_mimedata)) {
    mimedata->enqueue_now_ = true;
  }
  emit AddToPlaylistSignal(q_mimedata);

}

void ContextAlbumsView::OpenInNewPlaylist() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData *>(q_mimedata)) {
    mimedata->open_in_new_playlist_ = true;
  }
  emit AddToPlaylistSignal(q_mimedata);

}

void ContextAlbumsView::scrollTo(const QModelIndex &idx, ScrollHint hint) {

  if (is_in_keyboard_search_) {
    QTreeView::scrollTo(idx, QAbstractItemView::PositionAtTop);
  }
  else {
    QTreeView::scrollTo(idx, hint);
  }

}

SongList ContextAlbumsView::GetSelectedSongs() const {
  QModelIndexList selected_indexes = selectionModel()->selectedRows();
  return model_->GetChildSongs(selected_indexes);

}

void ContextAlbumsView::Organize() {

  if (!organize_dialog_) {
    organize_dialog_ = std::make_unique<OrganizeDialog>(app_->task_manager(), app_->collection_backend(), this);
  }

  organize_dialog_->SetDestinationModel(app_->collection_model()->directory_model());
  organize_dialog_->SetCopy(false);
  if (organize_dialog_->SetSongs(GetSelectedSongs())) {
    organize_dialog_->show();
  }
  else {
    QMessageBox::warning(this, tr("Error"), tr("None of the selected songs were suitable for copying to a device"));
  }

}

void ContextAlbumsView::EditTracks() {

  if (!edit_tag_dialog_) {
    edit_tag_dialog_ = std::make_unique<EditTagDialog>(app_, this);
  }
  edit_tag_dialog_->SetSongs(GetSelectedSongs());
  edit_tag_dialog_->show();

}

void ContextAlbumsView::CopyToDevice() {
#ifndef Q_OS_WIN
  if (!organize_dialog_) {
    organize_dialog_ = std::make_unique<OrganizeDialog>(app_->task_manager(), nullptr, this);
  }

  organize_dialog_->SetDestinationModel(app_->device_manager()->connected_devices_model(), true);
  organize_dialog_->SetCopy(true);
  organize_dialog_->SetSongs(GetSelectedSongs());
  organize_dialog_->show();
#endif

}

void ContextAlbumsView::ShowInBrowser() const {

  const SongList songs = GetSelectedSongs();
  QList<QUrl> urls;
  urls.reserve(songs.count());
  for (const Song &song : songs) {
    urls << song.url();
  }

  Utilities::OpenInFileBrowser(urls);

}
