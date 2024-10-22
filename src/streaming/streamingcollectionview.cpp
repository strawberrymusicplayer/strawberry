/*
 * Strawberry Music Player
 * This code was part of Clementine
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

#include <QWidget>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QVariant>
#include <QString>
#include <QPainter>
#include <QRect>
#include <QFont>
#include <QFontMetrics>
#include <QMimeData>
#include <QMenu>
#include <QtEvents>

#include "includes/shared_ptr.h"
#include "core/iconloader.h"
#include "core/mimedata.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilterwidget.h"
#include "collection/collectionitem.h"
#include "collection/collectionitemdelegate.h"
#include "streamingcollectionview.h"

using namespace Qt::Literals::StringLiterals;

StreamingCollectionView::StreamingCollectionView(QWidget *parent)
    : AutoExpandingTreeView(parent),
      collection_backend_(nullptr),
      collection_model_(nullptr),
      filter_(nullptr),
      favorite_(false),
      total_song_count_(0),
      total_artist_count_(0),
      total_album_count_(0),
      nomusic_(u":/pictures/nomusic.png"_s),
      context_menu_(nullptr),
      load_(nullptr),
      add_to_playlist_(nullptr),
      add_to_playlist_enqueue_(nullptr),
      add_to_playlist_enqueue_next_(nullptr),
      open_in_new_playlist_(nullptr),
      remove_songs_(nullptr),
      is_in_keyboard_search_(false) {

  setItemDelegate(new CollectionItemDelegate(this));
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setHeaderHidden(true);
  setAllColumnsShowFocus(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  SetAutoOpen(false);

  setStyleSheet(u"QTreeView::item{padding-top:1px;}"_s);

}

void StreamingCollectionView::Init(const SharedPtr<CollectionBackend> collection_backend, CollectionModel *collection_model, const bool favorite) {

  collection_backend_ = collection_backend;
  collection_model_ = collection_model;
  favorite_ = favorite;

  ReloadSettings();

}

void StreamingCollectionView::SetFilter(CollectionFilterWidget *filter) {

  filter_ = filter;

}

void StreamingCollectionView::ReloadSettings() {

  if (collection_model_) collection_model_->ReloadSettings();
  if (filter_) filter_->ReloadSettings();

}

void StreamingCollectionView::SaveFocus() {

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
      QModelIndex idx = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(current);
      SongList songs = collection_model_->GetChildSongs(idx);
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

void StreamingCollectionView::SaveContainerPath(const QModelIndex &child) {

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

void StreamingCollectionView::RestoreFocus() {

  if (last_selected_container_.isEmpty() && last_selected_song_.url().isEmpty()) {
    return;
  }
  RestoreLevelFocus();

}

bool StreamingCollectionView::RestoreLevelFocus(const QModelIndex &parent) {

  if (model()->canFetchMore(parent)) {
    model()->fetchMore(parent);
  }
  const int rows = model()->rowCount(parent);
  for (int i = 0; i < rows; i++) {
    const QModelIndex current = model()->index(i, 0, parent);
    if (!current.isValid()) continue;
    const QVariant role_type = model()->data(current, CollectionModel::Role_Type);
    if (!role_type.isValid()) continue;
    const CollectionItem::Type item_type = role_type.value<CollectionItem::Type>();
    switch (item_type) {
      case CollectionItem::Type::Root:
      case CollectionItem::Type::LoadingIndicator:
        break;
      case CollectionItem::Type::Song:
        if (!last_selected_song_.url().isEmpty()) {
          QModelIndex idx = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(current);
          const SongList songs = collection_model_->GetChildSongs(idx);
          for (const Song &song : songs) {
            if (song == last_selected_song_) {
              setCurrentIndex(current);
              return true;
            }
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
        if (last_selected_path_.contains(text)) {
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

void StreamingCollectionView::TotalSongCountUpdated(int count) {

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

void StreamingCollectionView::TotalArtistCountUpdated(int count) {

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

void StreamingCollectionView::TotalAlbumCountUpdated(int count) {

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

void StreamingCollectionView::paintEvent(QPaintEvent *event) {

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
    p.drawText(title_rect, Qt::AlignHCenter, tr("The streaming collection is empty!"));

    // Draw the other text
    p.setFont(QFont());

    QRect text_rect(0, title_rect.bottom() + 5, rect.width(), metrics.height());
    p.drawText(text_rect, Qt::AlignHCenter, tr("Click here to retrieve music"));
  }
  else {
    QTreeView::paintEvent(event);
  }

}

void StreamingCollectionView::mouseReleaseEvent(QMouseEvent *e) {

  QTreeView::mouseReleaseEvent(e);

  if (total_song_count_ == 0) {
    Q_EMIT GetSongs();
  }

}

void StreamingCollectionView::contextMenuEvent(QContextMenuEvent *e) {

  if (!context_menu_) {
    context_menu_ = new QMenu(this);
    add_to_playlist_ = context_menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Append to current playlist"), this, &StreamingCollectionView::AddToPlaylist);
    load_ = context_menu_->addAction(IconLoader::Load(u"media-playback-start"_s), tr("Replace current playlist"), this, &StreamingCollectionView::Load);
    open_in_new_playlist_ = context_menu_->addAction(IconLoader::Load(u"document-new"_s), tr("Open in new playlist"), this, &StreamingCollectionView::OpenInNewPlaylist);

    context_menu_->addSeparator();
    add_to_playlist_enqueue_ = context_menu_->addAction(IconLoader::Load(u"go-next"_s), tr("Queue track"), this, &StreamingCollectionView::AddToPlaylistEnqueue);
    add_to_playlist_enqueue_next_ = context_menu_->addAction(IconLoader::Load(u"go-next"_s), tr("Queue to play next"), this, &StreamingCollectionView::AddToPlaylistEnqueueNext);

    context_menu_->addSeparator();

    if (favorite_) {
      remove_songs_ = context_menu_->addAction(IconLoader::Load(u"edit-delete"_s), tr("Remove from favorites"), this, &StreamingCollectionView::RemoveSelectedSongs);
      context_menu_->addSeparator();
    }

    if (filter_) context_menu_->addMenu(filter_->menu());

  }

  context_menu_index_ = indexAt(e->pos());
  if (!context_menu_index_.isValid()) return;

  context_menu_index_ = qobject_cast<QSortFilterProxyModel*>(model())->mapToSource(context_menu_index_);
  QModelIndexList selected_indexes = qobject_cast<QSortFilterProxyModel*>(model())->mapSelectionToSource(selectionModel()->selection()).indexes();
  qint64 songs_selected = selected_indexes.count();

  // In all modes
  load_->setEnabled(songs_selected > 0);
  add_to_playlist_->setEnabled(songs_selected > 0);
  open_in_new_playlist_->setEnabled(songs_selected > 0);
  add_to_playlist_enqueue_->setEnabled(songs_selected > 0);
  if (remove_songs_) remove_songs_->setEnabled(songs_selected > 0);

  context_menu_->popup(e->globalPos());

}

void StreamingCollectionView::Load() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->clear_first_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void StreamingCollectionView::AddToPlaylist() {

  Q_EMIT AddToPlaylistSignal(model()->mimeData(selectedIndexes()));

}

void StreamingCollectionView::AddToPlaylistEnqueue() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_now_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void StreamingCollectionView::AddToPlaylistEnqueueNext() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->enqueue_next_now_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void StreamingCollectionView::OpenInNewPlaylist() {

  QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    mimedata->open_in_new_playlist_ = true;
  }
  Q_EMIT AddToPlaylistSignal(q_mimedata);

}

void StreamingCollectionView::RemoveSelectedSongs() {

  Q_EMIT RemoveSongs(GetSelectedSongs());

}

void StreamingCollectionView::keyboardSearch(const QString &search) {

  is_in_keyboard_search_ = true;
  QTreeView::keyboardSearch(search);
  is_in_keyboard_search_ = false;

}

void StreamingCollectionView::scrollTo(const QModelIndex &idx, ScrollHint hint) {

  if (is_in_keyboard_search_) {
    QTreeView::scrollTo(idx, QAbstractItemView::PositionAtTop);
  }
  else {
    QTreeView::scrollTo(idx, hint);
  }

}

SongList StreamingCollectionView::GetSelectedSongs() const {

  QModelIndexList selected_indexes = qobject_cast<QSortFilterProxyModel*>(model())->mapSelectionToSource(selectionModel()->selection()).indexes();
  return collection_model_->GetChildSongs(selected_indexes);

}

void StreamingCollectionView::FilterReturnPressed() {

  if (!currentIndex().isValid()) {
    // Pick the first thing that isn't a divider
    for (int row = 0; row < model()->rowCount(); ++row) {
      QModelIndex idx = model()->index(row, 0);
      const QVariant role_type = idx.data(CollectionModel::Role::Role_Type);
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

int StreamingCollectionView::TotalSongs() const {
  return total_song_count_;
}
int StreamingCollectionView::TotalArtists() const {
  return total_artist_count_;
}
int StreamingCollectionView::TotalAlbums() const {
  return total_album_count_;
}
