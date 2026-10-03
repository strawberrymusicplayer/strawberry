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

#include <algorithm>

#include <QtGlobal>
#include <QWidget>
#include <QTimer>
#include <QTreeView>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QMimeData>
#include <QVariant>
#include <QString>
#include <QPainter>
#include <QRect>
#include <QFont>
#include <QFontMetrics>
#include <QMenu>
#include <QAction>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>

#include "core/iconloader.h"
#include "core/mimedata.h"
#include "filterparser/filterparser.h"
#include "collectionmodel.h"
#include "collectionfilterwidget.h"
#include "collectionitem.h"
#include "collectionitemdelegate.h"
#include "collectiontreeview.h"

using namespace Qt::Literals::StringLiterals;

CollectionTreeView::CollectionTreeView(QWidget *parent, const int total_song_count)
    : AutoExpandingTreeView(parent),
      collection_model_(nullptr),
      filter_widget_(nullptr),
      total_song_count_(total_song_count),
      nomusic_(u":/pictures/nomusic.png"_s),
      context_menu_(nullptr),
      action_load_(nullptr),
      action_add_to_playlist_(nullptr),
      action_add_to_playlist_enqueue_(nullptr),
      action_add_to_playlist_enqueue_next_(nullptr),
      action_open_in_new_playlist_(nullptr),
      action_search_for_this_(nullptr),
      is_in_keyboard_search_(false),
      last_selected_container_expanded_(false),
      restore_focus_pending_(false),
      restoring_focus_(false),
      timer_restore_focus_(new QTimer(this)) {

  setItemDelegate(new CollectionItemDelegate(this));
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setHeaderHidden(true);
  setAllColumnsShowFocus(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::ExtendedSelection);

  setStyleSheet(u"QTreeView::item{padding-top:1px;}"_s);

  // Retry restoring at most once per event loop pass while songs are being added.
  timer_restore_focus_->setSingleShot(true);
  timer_restore_focus_->setInterval(0);
  QObject::connect(timer_restore_focus_, &QTimer::timeout, this, &CollectionTreeView::TryRestoreFocus);

}

void CollectionTreeView::SetCollectionModel(CollectionModel *collection_model) {

  if (collection_model_) {
    QObject::disconnect(collection_model_, &CollectionModel::rowsInserted, this, &CollectionTreeView::ScheduleRestoreFocus);
    QObject::disconnect(collection_model_, &CollectionModel::layoutChanged, this, &CollectionTreeView::ScheduleRestoreFocus);
  }

  collection_model_ = collection_model;

  if (collection_model_) {
    QObject::connect(collection_model_, &CollectionModel::rowsInserted, this, &CollectionTreeView::ScheduleRestoreFocus);
    QObject::connect(collection_model_, &CollectionModel::layoutChanged, this, &CollectionTreeView::ScheduleRestoreFocus);
  }

}

void CollectionTreeView::SetFilterWidget(CollectionFilterWidget *filter_widget) {

  filter_widget_ = filter_widget;

}

QSortFilterProxyModel *CollectionTreeView::filter_model() const {

  return qobject_cast<QSortFilterProxyModel*>(model());

}

bool CollectionTreeView::IsLoading() const {

  if (!collection_model_ || collection_model_->rowCount(QModelIndex()) != 1) return false;

  const QVariant role_type = collection_model_->index(0, 0).data(CollectionModel::Role_Type);
  return role_type.isValid() && role_type.value<CollectionItem::Type>() == CollectionItem::Type::LoadingIndicator;

}

void CollectionTreeView::paintEvent(QPaintEvent *event) {

  if (total_song_count_ == 0) {
    QPainter p(viewport());
    const QRect rect(viewport()->rect());

    // Draw the confused strawberry
    const QRect image_rect((rect.width() - nomusic_.width()) / 2, 50, nomusic_.width(), nomusic_.height());
    p.drawPixmap(image_rect, nomusic_);

    // Draw the title text
    QFont bold_font;
    bold_font.setBold(true);
    p.setFont(bold_font);

    const QFontMetrics metrics(bold_font);

    const QRect title_rect(0, image_rect.bottom() + 20, rect.width(), metrics.height());
    p.drawText(title_rect, Qt::AlignHCenter, EmptyTitleText());

    // Draw the other text
    p.setFont(QFont());

    const QRect text_rect(0, title_rect.bottom() + 5, rect.width(), metrics.height());
    p.drawText(text_rect, Qt::AlignHCenter, EmptyText());
  }
  else {
    QTreeView::paintEvent(event);
  }

}

void CollectionTreeView::mouseReleaseEvent(QMouseEvent *e) {

  QTreeView::mouseReleaseEvent(e);

  if (total_song_count_ == 0) {
    EmptyClicked();
  }

}

void CollectionTreeView::keyPressEvent(QKeyEvent *e) {

  switch (e->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
      if (currentIndex().isValid()) {
        Q_EMIT doubleClicked(currentIndex());
      }
      e->accept();
      break;
    default:
      break;
  }

  AutoExpandingTreeView::keyPressEvent(e);

}

void CollectionTreeView::contextMenuEvent(QContextMenuEvent *e) {

  if (!context_menu_) {
    context_menu_ = new QMenu(this);
    action_add_to_playlist_ = context_menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Append to current playlist"), this, &CollectionTreeView::AddToPlaylist);
    action_load_ = context_menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Replace current playlist"), this, &CollectionTreeView::Load);
    action_open_in_new_playlist_ = context_menu_->addAction(IconLoader::Load(u"document-new"_s), tr("Open in new playlist"), this, &CollectionTreeView::OpenInNewPlaylist);

    context_menu_->addSeparator();
    action_add_to_playlist_enqueue_ = context_menu_->addAction(IconLoader::Load(u"go-next"_s), tr("Queue track"), this, &CollectionTreeView::AddToPlaylistEnqueue);
    action_add_to_playlist_enqueue_next_ = context_menu_->addAction(IconLoader::Load(u"go-next"_s), tr("Queue to play next"), this, &CollectionTreeView::AddToPlaylistEnqueueNext);

    context_menu_->addSeparator();
    action_search_for_this_ = context_menu_->addAction(IconLoader::Load(u"edit-find"_s), tr("Search for this"), this, &CollectionTreeView::SearchForThis);

    context_menu_->addSeparator();
    AddContextMenuActions(context_menu_);

    if (filter_widget_) {
      context_menu_->addMenu(filter_widget_->menu());
    }
  }

  const QModelIndex idx = indexAt(e->pos());
  if (!idx.isValid() || !filter_model()) return;

  context_menu_index_ = filter_model()->mapToSource(idx);

  const bool has_selection = selectionModel() && selectionModel()->hasSelection();
  action_load_->setEnabled(has_selection);
  action_add_to_playlist_->setEnabled(has_selection);
  action_open_in_new_playlist_->setEnabled(has_selection);
  action_add_to_playlist_enqueue_->setEnabled(has_selection);
  action_add_to_playlist_enqueue_next_->setEnabled(has_selection);
  action_search_for_this_->setEnabled(has_selection && filter_widget_);

  UpdateContextMenuActions(has_selection);

  context_menu_->popup(e->globalPos());

}

QMimeData *CollectionTreeView::SelectedMimeData() const {

  if (!model() || !selectionModel() || !selectionModel()->hasSelection()) return nullptr;

  return model()->mimeData(selectedIndexes());

}

void CollectionTreeView::Load() {

  QMimeData *q_mimedata = SelectedMimeData();
  if (!q_mimedata) return;

  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->clear_first_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionTreeView::TotalSongCountUpdated(const int count) {

  if (count != total_song_count_) {
    total_song_count_ = count;
    update();
  }

  // An empty collection shows a text to click on.
  if (total_song_count_ == 0) {
    setCursor(Qt::PointingHandCursor);
  }
  else {
    unsetCursor();
  }

  Q_EMIT TotalSongCountUpdated_();

}

void CollectionTreeView::AddToPlaylist() {

  QMimeData *q_mimedata = SelectedMimeData();
  if (!q_mimedata) return;

  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionTreeView::AddToPlaylistEnqueue() {

  QMimeData *q_mimedata = SelectedMimeData();
  if (!q_mimedata) return;

  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_now_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionTreeView::AddToPlaylistEnqueueNext() {

  QMimeData *q_mimedata = SelectedMimeData();
  if (!q_mimedata) return;

  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_next_now_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionTreeView::OpenInNewPlaylist() {

  QMimeData *q_mimedata = SelectedMimeData();
  if (!q_mimedata) return;

  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->open_in_new_playlist_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void CollectionTreeView::SaveFocus() {

  if (!filter_model() || !collection_model_) return;

  // Reloading the collection first resets the model to show a loading indicator, and then resets it again with the loaded collection.
  // Keep what was saved before the first reset, since nothing can be selected while loading, or while the songs are still being added.
  if (IsLoading() || restore_focus_pending_) return;

  last_selected_song_ = Song();
  last_selected_container_ = SavedContainer();
  last_selected_container_expanded_ = false;
  last_selected_path_.clear();

  const QModelIndex current = currentIndex();
  const QVariant role_type = model()->data(current, CollectionModel::Role_Type);
  if (!role_type.isValid()) {
    return;
  }

  switch (role_type.value<CollectionItem::Type>()) {
    case CollectionItem::Type::Song:{
      const SongList songs = collection_model_->GetChildSongs(filter_model()->mapToSource(current));
      if (songs.isEmpty()) return;
      last_selected_song_ = songs.last();
      break;
    }

    case CollectionItem::Type::Container:
    case CollectionItem::Type::Divider:{
      last_selected_container_ = SavedContainer(model()->data(current, CollectionModel::Role_SortText).toString(), model()->data(current, CollectionModel::Role_ContainerType).toInt());
      last_selected_container_expanded_ = isExpanded(current);
      break;
    }

    default:
      return;
  }

  // Save the containers above the current item, from the top level down.
  for (QModelIndex parent = model()->parent(current); parent.isValid(); parent = model()->parent(parent)) {
    last_selected_path_.prepend(SavedContainer(model()->data(parent, CollectionModel::Role_SortText).toString(), model()->data(parent, CollectionModel::Role_ContainerType).toInt()));
  }

}

void CollectionTreeView::RestoreFocus() {

  if (last_selected_container_.sort_text.isEmpty() && last_selected_song_.url().isEmpty()) {
    restore_focus_pending_ = false;
    return;
  }

  // The songs are added to the model after the reset, so if the item is not there yet, try again as they are added.
  restore_focus_pending_ = true;
  TryRestoreFocus();

}

void CollectionTreeView::ScheduleRestoreFocus() {

  if (restore_focus_pending_ && !timer_restore_focus_->isActive()) {
    timer_restore_focus_->start();
  }

}

void CollectionTreeView::TryRestoreFocus() {

  if (!restore_focus_pending_ || !collection_model_) return;

  bool restored = false;
  restoring_focus_ = true;
  if (!last_selected_song_.url().isEmpty() && last_selected_song_.id() != -1) {
    // A song can be found by its ID wherever the grouping puts it.
    restored = RestoreSongFocus();
  }
  else if (SavedPathMatchesGrouping()) {
    restored = RestoreLevelFocus();
  }
  else {
    // The grouping changed, so the saved container doesn't exist anymore.
    restore_focus_pending_ = false;
  }
  restoring_focus_ = false;

  if (restored) {
    restore_focus_pending_ = false;
  }

}

bool CollectionTreeView::SavedPathMatchesGrouping() const {

  const CollectionModel::Grouping grouping = collection_model_->GetGroupBy();

  for (qsizetype level = 0; level < last_selected_path_.count(); ++level) {
    if (level >= 3 || static_cast<int>(grouping[static_cast<int>(level)]) != last_selected_path_.at(level).container_type) {
      return false;
    }
  }

  // A divider has no container type, it is not part of the grouping.
  const qsizetype level = last_selected_path_.count();
  if (last_selected_container_.container_type != static_cast<int>(CollectionModel::GroupBy::None)) {
    if (level >= 3 || static_cast<int>(grouping[static_cast<int>(level)]) != last_selected_container_.container_type) {
      return false;
    }
  }

  return true;

}

bool CollectionTreeView::RestoreSongFocus() {

  if (!filter_model() || !collection_model_) return false;

  const QModelIndex source_index = collection_model_->IndexOfSong(last_selected_song_.id());
  if (!source_index.isValid()) return false;

  // The song can be hidden by the filter.
  const QModelIndex proxy_index = filter_model()->mapFromSource(source_index);
  if (!proxy_index.isValid()) return false;

  for (QModelIndex parent = proxy_index.parent(); parent.isValid(); parent = parent.parent()) {
    expand(parent);
  }
  setCurrentIndex(proxy_index);

  return true;

}

void CollectionTreeView::currentChanged(const QModelIndex &current, const QModelIndex &previous) {

  AutoExpandingTreeView::currentChanged(current, previous);

  // The user selected another item, so don't move the focus to the restored item later.
  if (!restoring_focus_ && current.isValid()) {
    restore_focus_pending_ = false;
  }

}

bool CollectionTreeView::RestoreLevelFocus(const QModelIndex &parent, const qsizetype level) {

  if (!filter_model() || !collection_model_) return false;

  if (model()->canFetchMore(parent)) {
    model()->fetchMore(parent);
  }

  // Above the current item, only expand the container on the saved path for this level.
  // At the level of the current item, look for the saved song or container.
  const bool at_current_level = level >= last_selected_path_.count();

  const int rows = model()->rowCount(parent);
  for (int i = 0; i < rows; i++) {
    const QModelIndex current = model()->index(i, 0, parent);
    if (!current.isValid()) continue;
    const QVariant role_type = model()->data(current, CollectionModel::Role_Type);
    if (!role_type.isValid()) continue;
    switch (role_type.value<CollectionItem::Type>()) {
      case CollectionItem::Type::Root:
      case CollectionItem::Type::LoadingIndicator:
        break;

      case CollectionItem::Type::Song:
        if (at_current_level && !last_selected_song_.url().isEmpty()) {
          const SongList songs = collection_model_->GetChildSongs(filter_model()->mapToSource(current));
          if (std::any_of(songs.begin(), songs.end(), [this](const Song &song) { return song == last_selected_song_; })) {
            setCurrentIndex(current);
            return true;
          }
        }
        break;

      case CollectionItem::Type::Container:
      case CollectionItem::Type::Divider:{
        const QString text = model()->data(current, CollectionModel::Role_SortText).toString();
        const int container_type = model()->data(current, CollectionModel::Role_ContainerType).toInt();
        if (at_current_level) {
          if (!last_selected_container_.sort_text.isEmpty() && last_selected_container_.sort_text == text && last_selected_container_.container_type == container_type) {
            setExpanded(current, last_selected_container_expanded_);
            setCurrentIndex(current);
            return true;
          }
        }
        else if (last_selected_path_.at(level).sort_text == text && last_selected_path_.at(level).container_type == container_type) {
          expand(current);
          // Several containers can have the same text (happens with "unknown" all the time), so if the item is not found below this one, try the next.
          if (RestoreLevelFocus(current, level + 1)) {
            return true;
          }
          collapse(current);
        }
        break;
      }
    }
  }

  return false;

}

void CollectionTreeView::SearchForThis() {

  if (!filter_model() || !collection_model_ || !filter_widget_) return;

  const QModelIndex current = currentIndex();
  const QVariant role_type = model()->data(current, CollectionModel::Role_Type);
  if (!role_type.isValid()) {
    return;
  }

  const CollectionItem::Type item_type = role_type.value<CollectionItem::Type>();
  const QModelIndex idx = filter_model()->mapToSource(current);

  QString search;
  switch (item_type) {
    case CollectionItem::Type::Song:{
      const SongList songs = collection_model_->GetChildSongs(idx);
      if (songs.isEmpty()) return;
      search = u"title:"_s + FilterParser::QuoteValue(songs.last().title());
      break;
    }

    case CollectionItem::Type::Divider:
      // An empty search shows the whole collection.
      break;

    case CollectionItem::Type::Container:{
      CollectionItem *item = collection_model_->IndexToItem(idx);
      if (!item) return;
      const CollectionModel::GroupBy group_by = collection_model_->GetGroupBy()[item->container_level];
      while (!item->children.isEmpty()) {
        item = item->children.constFirst();
      }

      switch (group_by) {
        case CollectionModel::GroupBy::AlbumArtist:
          search = u"albumartist:"_s + FilterParser::QuoteValue(item->metadata.effective_albumartist());
          break;
        case CollectionModel::GroupBy::Artist:
          search = u"artist:"_s + FilterParser::QuoteValue(item->metadata.artist());
          break;
        case CollectionModel::GroupBy::Album:
        case CollectionModel::GroupBy::AlbumDisc:
          search = u"album:"_s + FilterParser::QuoteValue(item->metadata.album());
          break;
        case CollectionModel::GroupBy::YearAlbum:
        case CollectionModel::GroupBy::YearAlbumDisc:
          search = QStringLiteral("year:%1 album:").arg(item->metadata.year()) + FilterParser::QuoteValue(item->metadata.album());
          break;
        case CollectionModel::GroupBy::OriginalYearAlbum:
        case CollectionModel::GroupBy::OriginalYearAlbumDisc:
          search = QStringLiteral("year:%1 album:").arg(item->metadata.effective_originalyear()) + FilterParser::QuoteValue(item->metadata.album());
          break;
        case CollectionModel::GroupBy::Year:
          search = QStringLiteral("year:%1").arg(item->metadata.year());
          break;
        case CollectionModel::GroupBy::OriginalYear:
          search = QStringLiteral("year:%1").arg(item->metadata.effective_originalyear());
          break;
        case CollectionModel::GroupBy::Genre:
          search = u"genre:"_s + FilterParser::QuoteValue(item->metadata.genre());
          break;
        case CollectionModel::GroupBy::Composer:
          search = u"composer:"_s + FilterParser::QuoteValue(item->metadata.composer());
          break;
        case CollectionModel::GroupBy::Performer:
          search = u"performer:"_s + FilterParser::QuoteValue(item->metadata.performer());
          break;
        case CollectionModel::GroupBy::Grouping:
          search = u"grouping:"_s + FilterParser::QuoteValue(item->metadata.grouping());
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

  filter_widget_->ShowInCollection(search);

}

void CollectionTreeView::keyboardSearch(const QString &search) {

  is_in_keyboard_search_ = true;
  QTreeView::keyboardSearch(search);
  is_in_keyboard_search_ = false;

}

void CollectionTreeView::scrollTo(const QModelIndex &idx, ScrollHint hint) {

  if (is_in_keyboard_search_) {
    QTreeView::scrollTo(idx, QAbstractItemView::PositionAtTop);
  }
  else {
    QTreeView::scrollTo(idx, hint);
  }

}

SongList CollectionTreeView::GetSelectedSongs() const {

  if (!filter_model() || !collection_model_ || !selectionModel()) return SongList();

  const QModelIndexList selected_indexes = filter_model()->mapSelectionToSource(selectionModel()->selection()).indexes();
  return collection_model_->GetChildSongs(selected_indexes);

}

void CollectionTreeView::FilterReturnPressed() {

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
