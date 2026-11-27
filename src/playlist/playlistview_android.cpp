/*
 * Strawberry Music Player
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

// Android-only parts of PlaylistView, kept apart from playlistview.cpp to reduce conflicts when merging upstream.

#include <algorithm>

#include <QAbstractItemModel>
#include <QApplication>
#include <QDialog>
#include <QItemSelectionModel>
#include <QList>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QString>
#include <QUndoStack>

#include "playlistview.h"
#include "playlist.h"
#include "playlistfilter.h"
#include "playlistheader.h"
#include "playlistitem.h"
#include "queue/queue.h"
#include "widgets/actiondialog.h"
#include "widgets/longpresshandler.h"

using namespace Qt::Literals::StringLiterals;

void PlaylistView::SetupAndroidLongPress() {

  long_press_ = new LongPressHandler(LongPressHandler::PlatformInterval(), LongPressHandler::PlatformDragDistance(devicePixelRatioF()), this);
  viewport()->installEventFilter(this);
  QObject::connect(long_press_, &LongPressHandler::LongPressed, this, &PlaylistView::ShowAndroidSongActions);
  QObject::connect(qApp, &QApplication::applicationStateChanged, this, [this](const Qt::ApplicationState state) {
    if (state != Qt::ApplicationActive) CancelAndroidSongActions();
  });

}

void PlaylistView::PressAndroidLongPress(const QMouseEvent *event) {

  long_press_->Release();
  const QModelIndex pressed_index = indexAt(event->pos());
  if (event->button() == Qt::LeftButton && playlist_ && pressed_index.isValid() && pressed_index.column() != static_cast<int>(Playlist::Column::Rating)) {
    long_press_->Press(event->pos());
  }

}

void PlaylistView::DisconnectAndroidSongActions() {

  CancelAndroidSongActions();
  if (model()) {
    QObject::disconnect(model(), &QAbstractItemModel::layoutAboutToBeChanged, this, &PlaylistView::CancelAndroidSongActions);
    QObject::disconnect(model(), &QAbstractItemModel::rowsAboutToBeInserted, this, &PlaylistView::CancelAndroidSongActions);
    QObject::disconnect(model(), &QAbstractItemModel::rowsAboutToBeRemoved, this, &PlaylistView::CancelAndroidSongActions);
    QObject::disconnect(model(), &QAbstractItemModel::modelAboutToBeReset, this, &PlaylistView::CancelAndroidSongActions);
  }

}

void PlaylistView::ConnectAndroidSongActions() {

  QObject::connect(model(), &QAbstractItemModel::layoutAboutToBeChanged, this, &PlaylistView::CancelAndroidSongActions);
  QObject::connect(model(), &QAbstractItemModel::rowsAboutToBeInserted, this, &PlaylistView::CancelAndroidSongActions);
  QObject::connect(model(), &QAbstractItemModel::rowsAboutToBeRemoved, this, &PlaylistView::CancelAndroidSongActions);
  QObject::connect(model(), &QAbstractItemModel::modelAboutToBeReset, this, &PlaylistView::CancelAndroidSongActions);

}

void PlaylistView::UpdatePortraitColumns() {

  if (!header_state_restored_ || !screen()) return;
  const bool portrait = screen()->geometry().height() > screen()->geometry().width();
  const QSignalBlocker blocker(header_);
  header_->setContextMenuPolicy(Qt::PreventContextMenu);
  if (!portrait) {
    if (portrait_columns_) {
      portrait_columns_ = false;
      header_->RestoreState(header_state_);
      setHorizontalScrollBarPolicy(header_->is_stretch_enabled() ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    }
    return;
  }

  if (!portrait_columns_) {
    header_state_ = header_->SaveState();
    portrait_columns_ = true;
    header_->SetStretchEnabled(false);
    const QList<Playlist::Column> columns = {Playlist::Column::Track, Playlist::Column::Title, Playlist::Column::Artist, Playlist::Column::Album};
    for (int i = 0; i < header_->count(); ++i) {
      header_->setSectionHidden(i, !columns.contains(static_cast<Playlist::Column>(i)));
    }
    for (int i = 0; i < columns.count(); ++i) {
      header_->moveSection(header_->visualIndex(static_cast<int>(columns[i])), i);
    }
  }
  const int track_width = fontMetrics().horizontalAdvance(u"T#"_s) + 16;
  const int text_width = std::max(0, viewport()->width() - track_width) / 3;
  header_->resizeSection(static_cast<int>(Playlist::Column::Track), track_width);
  header_->resizeSection(static_cast<int>(Playlist::Column::Title), text_width);
  header_->resizeSection(static_cast<int>(Playlist::Column::Artist), text_width);
  header_->resizeSection(static_cast<int>(Playlist::Column::Album), std::max(0, viewport()->width() - track_width - 2 * text_width));
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

}
void PlaylistView::CancelAndroidSongActions() {

  if (long_press_) long_press_->Cancel();
  if (android_song_actions_ && android_song_actions_->isVisible()) android_song_actions_->reject();
  header_->CancelAndroidActions();

}

void PlaylistView::ShowAndroidSongActions(const QPoint position) {

  setState(QAbstractItemView::NoState);
  const QModelIndex index = indexAt(position);
  if (!playlist_ || !index.isValid() || !isVisible()) return;

  if (!selectionModel()->isRowSelected(index.row(), index.parent())) {
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  }
  const int count = static_cast<int>(selectionModel()->selectedRows().count());
  const QString title = count == 1 ? index.siblingAtColumn(static_cast<int>(Playlist::Column::Title)).data().toString() : tr("%n songs selected", "", count);
  ActionDialog *dialog = new ActionDialog(this, tr("Playlist actions"), title, ActionDialog::TitlePosition::AboveButtons);
  android_song_actions_ = dialog;
  const QModelIndex source_index = playlist_->filter()->mapToSource(index);
  const QModelIndexList selected_indexes = selectionModel()->selectedRows();
  QModelIndexList source_indexes;
  source_indexes.reserve(selected_indexes.count());
  int queued_count = 0;
  for (const QModelIndex &selected_index : selected_indexes) {
    const QModelIndex selected_source_index = playlist_->filter()->mapToSource(selected_index);
    if (!selected_source_index.isValid()) continue;
    source_indexes << selected_source_index;
    if (selected_source_index.data(Playlist::Role_QueuePosition).toInt() != -1) ++queued_count;
  }
  PlaylistItemPtr clicked_item = playlist_->item_at(source_index.row());
  const bool show_in_collection = clicked_item && clicked_item->IsLocalCollectionItem() &&
                                  clicked_item->EffectiveMetadata().id() != -1;
  QString queue_text = tr("Toggle queue status");
  if (queued_count == 0) queue_text = tr("Queue selected songs");
  else if (queued_count == source_indexes.count()) queue_text = tr("Remove selected songs from queue");
  QPushButton *stop_after = dialog->AddButton(tr("Stop after this track"));
  QPushButton *queue = dialog->AddButton(queue_text);
  QPushButton *queue_next = dialog->AddButton(tr("Queue selected tracks to play next"));
  QPushButton *show_collection = show_in_collection ? dialog->AddButton(tr("Show in collection")) : nullptr;
  QPushButton *remove = dialog->AddButton(count == 1 ? tr("Remove song") : tr("Remove %n songs", "", count));
  QPushButton *clear = dialog->AddButton(tr("Clear playlist"));
  dialog->AddCancelButton(tr("Cancel"));
  QObject::connect(selectionModel(), &QItemSelectionModel::selectionChanged, dialog, [dialog]() {
    if (dialog->isVisible()) dialog->reject();
  });
  QObject::connect(playlist_, &QObject::destroyed, dialog, &QDialog::reject);
  QObject::connect(stop_after, &QPushButton::clicked, dialog, [this, dialog, source_index]() {
    android_song_actions_.clear();
    dialog->accept();
    if (playlist_ && source_index.isValid()) playlist_->StopAfter(source_index.row());
  });
  QObject::connect(queue, &QPushButton::clicked, dialog, [this, dialog, source_indexes]() {
    android_song_actions_.clear();
    dialog->accept();
    if (playlist_ && !source_indexes.isEmpty()) playlist_->queue()->ToggleTracks(source_indexes);
  });
  QObject::connect(queue_next, &QPushButton::clicked, dialog, [this, dialog, source_indexes]() {
    android_song_actions_.clear();
    dialog->accept();
    if (playlist_ && !source_indexes.isEmpty()) playlist_->queue()->InsertFirst(source_indexes);
  });
  if (show_collection) {
    QObject::connect(show_collection, &QPushButton::clicked, dialog, [this, dialog]() {
      android_song_actions_.clear();
      dialog->accept();
      Q_EMIT AndroidShowInCollectionRequested();
    });
  }
  const auto perform = [this, dialog](const bool clear_playlist) {
    // Close before modifying the model, whose change signals cancel any pending actions.
    android_song_actions_.clear();
    dialog->accept();
    if (clear_playlist) {
      const bool undoable = playlist_->rowCount() <= Playlist::kUndoItemLimit;
      if (undoable) playlist_->undo_stack()->beginMacro(tr("Clear playlist"));
      playlist_->Clear();
      if (undoable) playlist_->undo_stack()->endMacro();
    }
    else {
      RemoveSelected();
    }
  };
  const auto choose = [this, dialog, count, perform](const bool clear_playlist) {
    const int removed_count = clear_playlist ? playlist_->rowCount() : count;
    if (removed_count <= Playlist::kUndoItemLimit) {
      perform(clear_playlist);
      return;
    }
    dialog->AskConfirmation(tr("Removing %n songs cannot be undone and will clear the undo history. Continue?", "", removed_count), clear_playlist ? tr("Clear playlist") : tr("Remove %n songs", "", removed_count), [perform, clear_playlist]() { perform(clear_playlist); });
  };
  QObject::connect(remove, &QPushButton::clicked, dialog, [choose]() { choose(false); });
  QObject::connect(clear, &QPushButton::clicked, dialog, [choose]() { choose(true); });
  dialog->Open();

}
