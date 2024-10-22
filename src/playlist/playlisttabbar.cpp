/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QWidget>
#include <QAbstractItemModel>
#include <QList>
#include <QVariant>
#include <QString>
#include <QIcon>
#include <QAction>
#include <QGridLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QRect>
#include <QSize>
#include <QToolTip>
#include <QLayoutItem>
#include <QMenu>
#include <QMessageBox>
#include <QTabBar>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QtEvents>
#include <QSettings>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/iconloader.h"
#include "core/mimedata.h"
#include "core/settings.h"
#include "widgets/favoritewidget.h"
#include "widgets/renametablineedit.h"
#include "playlist.h"
#include "playlistmanager.h"
#include "playlisttabbar.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "PlaylistTabBar";
constexpr int kDragHoverTimeout = 500;
}  // namespace

PlaylistTabBar::PlaylistTabBar(QWidget *parent)
    : QTabBar(parent),
      manager_(nullptr),
      menu_(new QMenu(this)),
      menu_index_(-1),
      action_star_(nullptr),
      action_close_(nullptr),
      action_rename_(nullptr),
      action_save_(nullptr),
      action_new_(nullptr),
      drag_hover_tab_(0),
      suppress_current_changed_(false),
      initialized_(false),
      rename_editor_(new RenameTabLineEdit(this)) {

  setAcceptDrops(true);
  setElideMode(Qt::ElideRight);
  setUsesScrollButtons(true);
  setTabsClosable(true);

  action_star_ = menu_->addAction(IconLoader::Load(u"star"_s), tr("Star playlist"), this, &PlaylistTabBar::StarSlot);
  action_close_ = menu_->addAction(IconLoader::Load(u"list-remove"_s), tr("Close playlist"), this, &PlaylistTabBar::CloseSlot);
  action_rename_ = menu_->addAction(IconLoader::Load(u"edit-rename"_s), tr("Rename playlist..."), this, &PlaylistTabBar::RenameSlot);
  action_save_ = menu_->addAction(IconLoader::Load(u"document-save"_s), tr("Save playlist..."), this, &PlaylistTabBar::SaveSlot);
  menu_->addSeparator();

  rename_editor_->setVisible(false);
  QObject::connect(rename_editor_, &RenameTabLineEdit::editingFinished, this, &PlaylistTabBar::RenameInline);
  QObject::connect(rename_editor_, &RenameTabLineEdit::EditingCanceled, this, &PlaylistTabBar::HideEditor);

  QObject::connect(this, &PlaylistTabBar::currentChanged, this, &PlaylistTabBar::CurrentIndexChanged);
  QObject::connect(this, &PlaylistTabBar::tabMoved, this, &PlaylistTabBar::TabMoved);
  // We can't just emit Close signal, we need to extract the playlist id first
  QObject::connect(this, &PlaylistTabBar::tabCloseRequested, this, &PlaylistTabBar::CloseFromTabIndex);

}

void PlaylistTabBar::SetActions(QAction *new_playlist, QAction *load_playlist) {

  menu_->insertAction(nullptr, new_playlist);
  menu_->insertAction(nullptr, load_playlist);

  action_new_ = new_playlist;

}

void PlaylistTabBar::SetManager(SharedPtr<PlaylistManager> manager) {

  manager_ = manager;
  QObject::connect(&*manager_, &PlaylistManager::PlaylistFavorited, this, &PlaylistTabBar::PlaylistFavoritedSlot);
  QObject::connect(&*manager_, &PlaylistManager::PlaylistManagerInitialized, this, &PlaylistTabBar::PlaylistManagerInitialized);

}

void PlaylistTabBar::PlaylistManagerInitialized() {

  // Signal that we are done loading and thus further changes should be committed to the db.
  initialized_ = true;
  QObject::disconnect(&*manager_, &PlaylistManager::PlaylistManagerInitialized, this, &PlaylistTabBar::PlaylistManagerInitialized);

}

void PlaylistTabBar::contextMenuEvent(QContextMenuEvent *e) {

  // We need to finish the renaming action before showing context menu
  if (rename_editor_->isVisible()) {
    //discard any change
    HideEditor();
  }

  menu_index_ = tabAt(e->pos());
  action_star_->setEnabled(menu_index_ != -1 && count() > 1);
  action_close_->setEnabled(menu_index_ != -1 && count() > 1);
  action_rename_->setEnabled(menu_index_ != -1);
  action_save_->setEnabled(menu_index_ != -1);

  menu_->popup(e->globalPos());

}

void PlaylistTabBar::mouseReleaseEvent(QMouseEvent *e) {

  if (e->button() == Qt::MiddleButton) {
    // Update menu index
    menu_index_ = tabAt(e->pos());
    CloseSlot();
  }

  QTabBar::mouseReleaseEvent(e);

}

void PlaylistTabBar::mouseDoubleClickEvent(QMouseEvent *e) {

  int index = tabAt(e->pos());

  // Discard a double click with the middle button
  if (e->button() != Qt::MiddleButton) {
    if (index == -1) {
      action_new_->activate(QAction::Trigger);
    }
    else {
      menu_index_ = index;
      rename_editor_->setGeometry(tabRect(index));
      rename_editor_->setText(manager_->GetPlaylistName(tabData(index).toInt()));
      rename_editor_->setVisible(true);
      rename_editor_->setFocus();
    }
  }

  QTabBar::mouseDoubleClickEvent(e);

}

void PlaylistTabBar::RenameSlot() {

  if (menu_index_ == -1) return;

  const int playlist_id = tabData(menu_index_).toInt();
  const QString old_name = manager_->GetPlaylistName(playlist_id);
  const QString new_name = QInputDialog::getText(this, tr("Rename playlist"), tr("Enter a new name for this playlist"), QLineEdit::Normal, old_name);

  if (new_name.isEmpty() || new_name == old_name) return;

  Q_EMIT Rename(playlist_id, new_name);

}

void PlaylistTabBar::RenameInline() {
  Q_EMIT Rename(tabData(menu_index_).toInt(), rename_editor_->text());
  HideEditor();
}

void PlaylistTabBar::HideEditor() {

  // editingFinished() will be called twice due to Qt bug #40, so we reuse the same instance, don't delete it
  rename_editor_->setVisible(false);

  // Hack to give back focus to playlist view
  manager_->SetCurrentPlaylist(manager_->current()->id());

}

void PlaylistTabBar::StarSlot() {

  if (menu_index_ == -1) return;

  FavoriteWidget *favorite_widget = qobject_cast<FavoriteWidget*>(tabButton(menu_index_, QTabBar::LeftSide));
  if (favorite_widget) {
    favorite_widget->SetFavorite(!favorite_widget->IsFavorite());
  }

}

void PlaylistTabBar::CloseSlot() {

  if (menu_index_ == -1) return;

  const int playlist_id = tabData(menu_index_).toInt();

  Settings s;
  s.beginGroup(kSettingsGroup);

  const bool ask_for_delete = s.value("warn_close_playlist", true).toBool();

  if (ask_for_delete && !manager_->IsPlaylistFavorite(playlist_id) && !manager_->playlist(playlist_id)->GetAllSongs().empty()) {
    QMessageBox confirmation_box;
    confirmation_box.setWindowIcon(QIcon(u":/icons/64x64/strawberry.png"_s));
    confirmation_box.setWindowTitle(tr("Remove playlist"));
    confirmation_box.setIcon(QMessageBox::Question);
    confirmation_box.setText(
        tr("You are about to remove a playlist which is not part of your "
           "favorite playlists: "
           "the playlist will be deleted (this action cannot be undone). \n"
           "Are you sure you want to continue?"));
    confirmation_box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

    QCheckBox dont_prompt_again(tr("Warn me when closing a playlist tab"), &confirmation_box);
    dont_prompt_again.setChecked(ask_for_delete);
    dont_prompt_again.blockSignals(true);
    dont_prompt_again.setToolTip(tr("This option can be changed in the \"Behavior\" preferences"));

    QGridLayout *grid = qobject_cast<QGridLayout*>(confirmation_box.layout());
    QDialogButtonBox *buttons = confirmation_box.findChild<QDialogButtonBox*>();
    if (grid && buttons) {
      const int index = grid->indexOf(buttons);
      int row = 0, column = 0, row_span = 0, column_span = 0;
      grid->getItemPosition(index, &row, &column, &row_span, &column_span);
      QLayoutItem *buttonsItem = grid->takeAt(index);
      grid->addWidget(&dont_prompt_again, row, column, row_span, column_span, Qt::AlignLeft | Qt::AlignTop);
      grid->addItem(buttonsItem, ++row, column, row_span, column_span);
    }
    else {
      confirmation_box.addButton(&dont_prompt_again, QMessageBox::ActionRole);
    }

    if (confirmation_box.exec() != QMessageBox::Yes) {
      return;
    }

    // If user changed the pref, save the new one
    if (dont_prompt_again.isChecked() != ask_for_delete) {
      s.setValue("warn_close_playlist", dont_prompt_again.isChecked());
    }
  }

  // Close the playlist. If the playlist is not a favorite playlist, it will be deleted, as it will not be visible after being closed.
  // Otherwise, the tab is closed but the playlist still exists and can be resurrected from the "Playlists" tab.
  Q_EMIT Close(playlist_id);

  // Select the nearest tab.
  if (menu_index_ > 1) {
    setCurrentIndex(menu_index_ - 1);
  }

  // Update playlist tab order/visibility
  TabMoved();

}

void PlaylistTabBar::CloseFromTabIndex(int index) {

  // Update the global index
  menu_index_ = index;
  CloseSlot();

}

void PlaylistTabBar::SaveSlot() {

  if (menu_index_ == -1) return;

  Q_EMIT Save(tabData(menu_index_).toInt());

}

int PlaylistTabBar::current_id() const {
  if (currentIndex() == -1) return -1;
  return tabData(currentIndex()).toInt();
}

int PlaylistTabBar::index_of(const int id) const {

  for (int i = 0; i < count(); ++i) {
    if (tabData(i).toInt() == id) {
      return i;
    }
  }
  return -1;

}

void PlaylistTabBar::set_current_id(const int id) { setCurrentIndex(index_of(id)); }

int PlaylistTabBar::id_of(const int index) const {

  if (index < 0 || index >= count()) {
    qLog(Warning) << "Playlist tab index requested is out of bounds!";
    return 0;
  }
  return tabData(index).toInt();

}

void PlaylistTabBar::set_icon_by_id(const int id, const QIcon &icon) {
  setTabIcon(index_of(id), icon);
}

void PlaylistTabBar::RemoveTab(const int id) {
  removeTab(index_of(id));
}

void PlaylistTabBar::set_text_by_id(const int id, const QString &text) {

  QString new_text = text;
  new_text = new_text.replace(u'&', "&&"_L1);
  setTabText(index_of(id), new_text);
  setTabToolTip(index_of(id), text);

}

void PlaylistTabBar::CurrentIndexChanged(const int index) {
  if (!suppress_current_changed_) Q_EMIT CurrentIdChanged(tabData(index).toInt());
}

void PlaylistTabBar::InsertTab(const int id, const int index, const QString &text, const bool favorite) {

  QString new_text = text;
  if (new_text.contains(u'&')) {
    new_text = new_text.replace(u'&', "&&"_L1);
  }

  suppress_current_changed_ = true;
  insertTab(index, new_text);
  setTabData(index, id);
  setTabToolTip(index, text);
  FavoriteWidget *widget = new FavoriteWidget(id, favorite);
  widget->setToolTip(tr("Double-click here to favorite this playlist so it will be saved and remain accessible through the \"Playlists\" panel on the left side bar"));
  QObject::connect(widget, &FavoriteWidget::FavoriteStateChanged, this, &PlaylistTabBar::PlaylistFavorited);
  setTabButton(index, QTabBar::LeftSide, widget);
  suppress_current_changed_ = false;

  // If we are still starting up, we don't need to do this, as the tab ordering after startup will be the same as was already in the db.
  if (initialized_) {
    if (currentIndex() == index) Q_EMIT CurrentIdChanged(id);

    // Update playlist tab order/visibility
    TabMoved();
  }

}

void PlaylistTabBar::TabMoved() {

  QList<int> ids;
  ids.reserve(count());
  for (int i = 0; i < count(); ++i) {
    ids << tabData(i).toInt();
  }
  Q_EMIT PlaylistOrderChanged(ids);

}

void PlaylistTabBar::dragEnterEvent(QDragEnterEvent *e) {
  if (e->mimeData()->hasUrls() || e->mimeData()->hasFormat(QString::fromLatin1(Playlist::kRowsMimetype)) || qobject_cast<const MimeData*>(e->mimeData())) {
    e->acceptProposedAction();
  }
}

void PlaylistTabBar::dragMoveEvent(QDragMoveEvent *e) {

  drag_hover_tab_ = tabAt(e->position().toPoint());

  if (drag_hover_tab_ != -1) {
    e->setDropAction(Qt::CopyAction);
    e->accept(tabRect(drag_hover_tab_));

    if (!drag_hover_timer_.isActive()) {
      drag_hover_timer_.start(kDragHoverTimeout, this);
    }
  }
  else {
    drag_hover_timer_.stop();
  }

}

void PlaylistTabBar::dragLeaveEvent(QDragLeaveEvent *e) {
  Q_UNUSED(e)
  drag_hover_timer_.stop();
}

void PlaylistTabBar::timerEvent(QTimerEvent *e) {

  QTabBar::timerEvent(e);

  if (e->timerId() == drag_hover_timer_.timerId()) {
    drag_hover_timer_.stop();
    if (drag_hover_tab_ != -1) setCurrentIndex(drag_hover_tab_);
  }
}

void PlaylistTabBar::dropEvent(QDropEvent *e) {

  if (drag_hover_tab_ == -1) {
    const MimeData *mime_data = qobject_cast<const MimeData*>(e->mimeData());
    if (mime_data && !mime_data->name_for_new_playlist_.isEmpty()) {
      manager_->New(mime_data->name_for_new_playlist_);
    }
    else {
      manager_->New(tr("Playlist"));
    }
    setCurrentIndex(count() - 1);
  }
  else {
    setCurrentIndex(drag_hover_tab_);
  }

  manager_->current()->dropMimeData(e->mimeData(), e->proposedAction(), -1, 0, QModelIndex());

}

bool PlaylistTabBar::event(QEvent *e) {

  switch (e->type()) {
    case QEvent::ToolTip:{
      QHelpEvent *he = static_cast<QHelpEvent*>(e);

      QSize real_tab = tabSizeHint(tabAt(he->pos()));
      QRect displayed_tab = tabRect(tabAt(he->pos()));
      // Check whether the tab is elided or not
      bool is_elided = displayed_tab.width() < real_tab.width();
      if (!is_elided) {
        // If it's not elided, don't show the tooltip
        QToolTip::hideText();
      }
      else {
        QToolTip::showText(he->globalPos(), tabToolTip(tabAt(he->pos())));
      }
      return true;
    }
    default:
      return QTabBar::event(e);
  }

}

void PlaylistTabBar::PlaylistFavoritedSlot(const int id, const bool favorite) {

  const int index = index_of(id);
  FavoriteWidget *favorite_widget = qobject_cast<FavoriteWidget*>(tabButton(index, QTabBar::LeftSide));
  if (favorite_widget) {
    favorite_widget->SetFavorite(favorite);
  }

}
