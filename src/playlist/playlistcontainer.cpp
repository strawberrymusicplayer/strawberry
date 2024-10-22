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

#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QScrollBar>
#include <QAction>
#include <QPoint>
#include <QString>
#include <QSize>
#include <QFont>
#include <QIcon>
#include <QColor>
#include <QFrame>
#include <QPalette>
#include <QTimer>
#include <QTimeLine>
#include <QFileDialog>
#include <QLabel>
#include <QKeySequence>
#include <QToolButton>
#include <QUndoStack>
#include <QtEvents>
#include <QSettings>

#include "includes/shared_ptr.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "filterparser/filterparser.h"
#include "playlist.h"
#include "playlisttabbar.h"
#include "playlistview.h"
#include "playlistcontainer.h"
#include "playlistmanager.h"
#include "playlistfilter.h"
#include "playlistparsers/playlistparser.h"
#include "ui_playlistcontainer.h"
#include "widgets/searchfield.h"
#include "constants/appearancesettings.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "Playlist";
constexpr int kFilterDelayMs = 100;
constexpr int kFilterDelayPlaylistSizeThreshold = 5000;
}  // namespace

PlaylistContainer::PlaylistContainer(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_PlaylistContainer),
      manager_(nullptr),
      undo_(nullptr),
      redo_(nullptr),
      playlist_(nullptr),
      starting_up_(true),
      tab_bar_visible_(false),
      tab_bar_animation_(new QTimeLine(500, this)),
      no_matches_label_(nullptr),
      filter_timer_(new QTimer(this)) {

  ui_->setupUi(this);

  no_matches_label_ = new QLabel(ui_->playlist);
  no_matches_label_->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  no_matches_label_->setAttribute(Qt::WA_TransparentForMouseEvents);
  no_matches_label_->setWordWrap(true);
  no_matches_label_->raise();
  no_matches_label_->hide();

  // Set the colour of the no matches label to the disabled text colour
  QPalette no_matches_palette = no_matches_label_->palette();
  const QColor no_matches_color = no_matches_palette.color(QPalette::Disabled, QPalette::Text);
  no_matches_palette.setColor(QPalette::Normal, QPalette::WindowText, no_matches_color);
  no_matches_palette.setColor(QPalette::Inactive, QPalette::WindowText, no_matches_color);
  no_matches_label_->setPalette(no_matches_palette);

  // Remove QFrame border
  ui_->toolbar->setStyleSheet(u"QFrame { border: 0px; }"_s);

  // Make it bold
  QFont no_matches_font = no_matches_label_->font();
  no_matches_font.setBold(true);
  no_matches_label_->setFont(no_matches_font);

  settings_.beginGroup(kSettingsGroup);

  // Tab bar
  ui_->tab_bar->setExpanding(false);
  ui_->tab_bar->setMovable(true);

  QObject::connect(tab_bar_animation_, &QTimeLine::frameChanged, this, &PlaylistContainer::SetTabBarHeight);
  ui_->tab_bar->setMaximumHeight(0);

  // Connections
  QObject::connect(ui_->tab_bar, &PlaylistTabBar::currentChanged, this, &PlaylistContainer::Save);
  QObject::connect(ui_->tab_bar, &PlaylistTabBar::Save, this, &PlaylistContainer::SaveCurrentPlaylist);

  // set up timer for delayed filter updates
  filter_timer_->setSingleShot(true);
  filter_timer_->setInterval(kFilterDelayMs);
  QObject::connect(filter_timer_, &QTimer::timeout, this, &PlaylistContainer::UpdateFilter);

  // Replace playlist search filter with native search box.
  QObject::connect(ui_->search_field, &SearchField::textChanged, this, &PlaylistContainer::MaybeUpdateFilter);
  QObject::connect(ui_->playlist, &PlaylistView::FocusOnFilterSignal, this, &PlaylistContainer::FocusOnFilter);
  ui_->search_field->installEventFilter(this);

  ui_->search_field->setToolTip(FilterParser::ToolTip());

  ReloadSettings();

}

PlaylistContainer::~PlaylistContainer() { delete ui_; }

PlaylistView *PlaylistContainer::view() const { return ui_->playlist; }

void PlaylistContainer::SetActions(QAction *new_playlist, QAction *load_playlist, QAction *save_playlist, QAction *clear_playlist, QAction *next_playlist, QAction *previous_playlist, QAction *save_all_playlists) {

  ui_->create_new->setDefaultAction(new_playlist);
  ui_->load->setDefaultAction(load_playlist);
  ui_->save->setDefaultAction(save_playlist);
  ui_->clear->setDefaultAction(clear_playlist);

  ui_->tab_bar->SetActions(new_playlist, load_playlist);

  QObject::connect(new_playlist, &QAction::triggered, this, &PlaylistContainer::NewPlaylist);
  QObject::connect(save_playlist, &QAction::triggered, this, &PlaylistContainer::SaveCurrentPlaylist);
  QObject::connect(load_playlist, &QAction::triggered, this, &PlaylistContainer::LoadPlaylist);
  QObject::connect(clear_playlist, &QAction::triggered, this, &PlaylistContainer::ClearPlaylist);
  QObject::connect(next_playlist, &QAction::triggered, this, &PlaylistContainer::GoToNextPlaylistTab);
  QObject::connect(previous_playlist, &QAction::triggered, this, &PlaylistContainer::GoToPreviousPlaylistTab);
  QObject::connect(clear_playlist, &QAction::triggered, this, &PlaylistContainer::ClearPlaylist);
  QObject::connect(save_all_playlists, &QAction::triggered, &*manager_, &PlaylistManager::SaveAllPlaylists);

}

void PlaylistContainer::SetManager(SharedPtr<PlaylistManager> manager) {

  manager_ = manager;
  ui_->tab_bar->SetManager(manager);

  QObject::connect(ui_->tab_bar, &PlaylistTabBar::CurrentIdChanged, &*manager, &PlaylistManager::SetCurrentPlaylist);
  QObject::connect(ui_->tab_bar, &PlaylistTabBar::Rename, &*manager, &PlaylistManager::Rename);
  QObject::connect(ui_->tab_bar, &PlaylistTabBar::Close, &*manager, &PlaylistManager::Close);
  QObject::connect(ui_->tab_bar, &PlaylistTabBar::PlaylistFavorited, &*manager, &PlaylistManager::Favorite);

  QObject::connect(ui_->tab_bar, &PlaylistTabBar::PlaylistOrderChanged, &*manager, &PlaylistManager::ChangePlaylistOrder);

  QObject::connect(&*manager, &PlaylistManager::CurrentChanged, this, &PlaylistContainer::SetViewModel);
  QObject::connect(&*manager, &PlaylistManager::PlaylistAdded, this, &PlaylistContainer::PlaylistAdded);
  QObject::connect(&*manager, &PlaylistManager::PlaylistManagerInitialized, this, &PlaylistContainer::Started);
  QObject::connect(&*manager, &PlaylistManager::PlaylistClosed, this, &PlaylistContainer::PlaylistClosed);
  QObject::connect(&*manager, &PlaylistManager::PlaylistRenamed, this, &PlaylistContainer::PlaylistRenamed);

}

void PlaylistContainer::SetViewModel(Playlist *playlist, const int scroll_position) {

  if (view()->selectionModel()) {
    QObject::disconnect(view()->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PlaylistContainer::SelectionChanged);
  }
  if (playlist_ && playlist_->filter()) {
    QObject::disconnect(playlist_->filter(), &QSortFilterProxyModel::modelReset, this, &PlaylistContainer::UpdateNoMatchesLabel);
    QObject::disconnect(playlist_->filter(), &QSortFilterProxyModel::rowsInserted, this, &PlaylistContainer::UpdateNoMatchesLabel);
    QObject::disconnect(playlist_->filter(), &QSortFilterProxyModel::rowsRemoved, this, &PlaylistContainer::UpdateNoMatchesLabel);
  }
  if (playlist_) {
    QObject::disconnect(playlist_, &Playlist::modelReset, this, &PlaylistContainer::UpdateNoMatchesLabel);
    QObject::disconnect(playlist_, &Playlist::rowsInserted, this, &PlaylistContainer::UpdateNoMatchesLabel);
    QObject::disconnect(playlist_, &Playlist::rowsRemoved, this, &PlaylistContainer::UpdateNoMatchesLabel);
  }

  playlist_ = playlist;

  // Set the view
  playlist->IgnoreSorting(true);
  view()->setModel(playlist->filter());
  view()->SetPlaylist(playlist);
  view()->selectionModel()->select(manager_->current_selection(), QItemSelectionModel::ClearAndSelect);
  if (scroll_position != 0) view()->verticalScrollBar()->setValue(scroll_position);
  playlist->IgnoreSorting(false);

  QObject::connect(view()->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PlaylistContainer::SelectionChanged);
  Q_EMIT ViewSelectionModelChanged();

  // Update filter
  ui_->search_field->setText(playlist->filter()->filter_string());

  // Update the no matches label
  QObject::connect(playlist_->filter(), &QSortFilterProxyModel::modelReset, this, &PlaylistContainer::UpdateNoMatchesLabel);
  QObject::connect(playlist_->filter(), &QSortFilterProxyModel::rowsInserted, this, &PlaylistContainer::UpdateNoMatchesLabel);
  QObject::connect(playlist_->filter(), &QSortFilterProxyModel::rowsRemoved, this, &PlaylistContainer::UpdateNoMatchesLabel);
  QObject::connect(playlist_, &Playlist::modelReset, this, &PlaylistContainer::UpdateNoMatchesLabel);
  QObject::connect(playlist_, &Playlist::rowsInserted, this, &PlaylistContainer::UpdateNoMatchesLabel);
  QObject::connect(playlist_, &Playlist::rowsRemoved, this, &PlaylistContainer::UpdateNoMatchesLabel);
  UpdateNoMatchesLabel();

  // Ensure that tab is current
  if (ui_->tab_bar->current_id() != manager_->current_id()) {
    ui_->tab_bar->set_current_id(manager_->current_id());
  }

  // Sort out the undo/redo actions
  delete undo_;
  delete redo_;
  undo_ = playlist->undo_stack()->createUndoAction(this, tr("Undo"));
  redo_ = playlist->undo_stack()->createRedoAction(this, tr("Redo"));
  undo_->setIcon(IconLoader::Load(u"edit-undo"_s));
  undo_->setShortcut(QKeySequence::Undo);
  redo_->setIcon(IconLoader::Load(u"edit-redo"_s));
  redo_->setShortcut(QKeySequence::Redo);

  ui_->undo->setDefaultAction(undo_);
  ui_->redo->setDefaultAction(redo_);

  Q_EMIT UndoRedoActionsChanged(undo_, redo_);

}

void PlaylistContainer::ReloadSettings() {

  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  int iconsize = s.value(AppearanceSettings::kIconSizePlaylistButtons, 20).toInt();
  s.endGroup();

  ui_->create_new->setIconSize(QSize(iconsize, iconsize));
  ui_->load->setIconSize(QSize(iconsize, iconsize));
  ui_->save->setIconSize(QSize(iconsize, iconsize));
  ui_->clear->setIconSize(QSize(iconsize, iconsize));
  ui_->undo->setIconSize(QSize(iconsize, iconsize));
  ui_->redo->setIconSize(QSize(iconsize, iconsize));
  ui_->search_field->setIconSize(iconsize);

  bool playlist_clear = settings_.value("playlist_clear", true).toBool();
  if (playlist_clear) {
    ui_->clear->show();
  }
  else {
    ui_->clear->hide();
  }

  bool show_toolbar = settings_.value("show_toolbar", true).toBool();
  ui_->toolbar->setVisible(show_toolbar);

  if (!show_toolbar) ui_->search_field->clear();

}

bool PlaylistContainer::SearchFieldHasFocus() const {
  return ui_->toolbar->isVisible() && ui_->search_field->hasFocus();
}

void PlaylistContainer::FocusSearchField() {
  if (ui_->toolbar->isVisible()) ui_->search_field->setFocus();
}

void PlaylistContainer::ActivePlaying() {
  UpdateActiveIcon(QIcon(u":/pictures/tiny-play.png"_s));
}

void PlaylistContainer::ActivePaused() {
  UpdateActiveIcon(QIcon(u":/pictures/tiny-pause.png"_s));
}

void PlaylistContainer::ActiveStopped() { UpdateActiveIcon(QIcon()); }

void PlaylistContainer::UpdateActiveIcon(const QIcon &icon) {

  // Unset all existing icons
  for (int i = 0; i < ui_->tab_bar->count(); ++i) {
    ui_->tab_bar->setTabIcon(i, QIcon());
  }

  // Set our icon
  if (!icon.isNull()) ui_->tab_bar->set_icon_by_id(manager_->active_id(), icon);

}

void PlaylistContainer::PlaylistAdded(const int id, const QString &name, const bool favorite) {

  const int index = ui_->tab_bar->count();
  ui_->tab_bar->InsertTab(id, index, name, favorite);

  // Are we start up, should we select this tab?
  if (starting_up_ && settings_.value("current_playlist", 1).toInt() == id) {
    starting_up_ = false;
    ui_->tab_bar->set_current_id(id);
  }

  if (ui_->tab_bar->count() > 1) {
    // Have to do this here because sizeHint() is only valid when there's a tab in the bar.
    tab_bar_animation_->setFrameRange(0, ui_->tab_bar->sizeHint().height());

    if (!isVisible()) {
      // Skip the animation since the window is hidden (e.g. if we're still loading the UI).
      tab_bar_visible_ = true;
      ui_->tab_bar->setMaximumHeight(tab_bar_animation_->endFrame());
    }
    else {
      SetTabBarVisible(true);
    }
  }

}

void PlaylistContainer::Started() { starting_up_ = false; }

void PlaylistContainer::PlaylistClosed(const int id) {

  ui_->tab_bar->RemoveTab(id);

  if (ui_->tab_bar->count() <= 1) SetTabBarVisible(false);

}

void PlaylistContainer::PlaylistRenamed(const int id, const QString &new_name) {
  ui_->tab_bar->set_text_by_id(id, new_name);
}

void PlaylistContainer::NewPlaylist() { manager_->New(tr("Playlist")); }

void PlaylistContainer::LoadPlaylist() {

  QString filename = settings_.value("last_load_playlist").toString();
  filename = QFileDialog::getOpenFileName(this, tr("Load playlist"), filename, manager_->parser()->filters(PlaylistParser::Type::Load));

  if (filename.isNull()) return;

  settings_.setValue("last_load_playlist", filename);

  manager_->Load(filename);

}

void PlaylistContainer::SavePlaylist(const int id) {

  // Use the tab name as the suggested name
  QString suggested_name = ui_->tab_bar->tabText(ui_->tab_bar->currentIndex());

  manager_->SaveWithUI(id, suggested_name);

}

void PlaylistContainer::ClearPlaylist() {}

void PlaylistContainer::GoToNextPlaylistTab() {

  // Get the next tab's id
  int id_next = ui_->tab_bar->id_of((ui_->tab_bar->currentIndex() + 1) % ui_->tab_bar->count());
  // Switch to next tab
  manager_->SetCurrentPlaylist(id_next);

}

void PlaylistContainer::GoToPreviousPlaylistTab() {

  // Get the next tab's id
  int id_previous = ui_->tab_bar->id_of((ui_->tab_bar->currentIndex() + ui_->tab_bar->count() - 1) % ui_->tab_bar->count());
  // Switch to next tab
  manager_->SetCurrentPlaylist(id_previous);

}

void PlaylistContainer::Save() {

  if (starting_up_) return;

  settings_.setValue("current_playlist", ui_->tab_bar->current_id());

}

void PlaylistContainer::SetTabBarVisible(const bool visible) {

  if (tab_bar_visible_ == visible) return;
  tab_bar_visible_ = visible;

  tab_bar_animation_->setDirection(visible ? QTimeLine::Forward : QTimeLine::Backward);
  tab_bar_animation_->start();

}

void PlaylistContainer::SetTabBarHeight(const int height) {
  ui_->tab_bar->setMaximumHeight(height);
}

void PlaylistContainer::MaybeUpdateFilter() {

  if (!ui_->toolbar->isVisible()) return;

  // delaying the filter update on small playlists is undesirable and an empty filter applies very quickly, too
  if (manager_->current()->rowCount() < kFilterDelayPlaylistSizeThreshold || ui_->search_field->text().isEmpty()) {
    UpdateFilter();
  }
  else {
    filter_timer_->start();
  }

}

void PlaylistContainer::UpdateFilter() {

  if (!ui_->toolbar->isVisible()) return;

  manager_->current()->filter()->SetFilterString(ui_->search_field->text());
  ui_->playlist->JumpToCurrentlyPlayingTrack();

  UpdateNoMatchesLabel();

}

void PlaylistContainer::UpdateNoMatchesLabel() {

  Playlist *playlist = manager_->current();
  const bool has_rows = playlist->rowCount() != 0;
  const bool has_results = playlist->filter()->rowCount() != 0;

  QString text;
  if (has_rows && !has_results) {
    text = tr("No matches found.  Clear the search box to show the whole playlist again.");
  }

  if (!text.isEmpty()) {
    no_matches_label_->setText(text);
    RepositionNoMatchesLabel(true);
    no_matches_label_->show();
  }
  else {
    no_matches_label_->hide();
  }

}

void PlaylistContainer::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  RepositionNoMatchesLabel();
}

void PlaylistContainer::FocusOnFilter(QKeyEvent *event) {

  if (ui_->toolbar->isVisible()) {
    ui_->search_field->setFocus();
    switch (event->key()) {
      case Qt::Key_Backspace:
        break;
      case Qt::Key_Escape:
        ui_->search_field->clear();
        break;
      default:
        ui_->search_field->setText(ui_->search_field->text() + event->text());
        break;
    }
  }

}

void PlaylistContainer::RepositionNoMatchesLabel(const bool force) {

  if (!force && !no_matches_label_->isVisible()) return;

  const int kBorder = 10;

  QPoint pos = ui_->playlist->viewport()->mapTo(ui_->playlist, QPoint(kBorder, kBorder));
  QSize size = ui_->playlist->viewport()->size();
  size.setWidth(size.width() - kBorder * 2);
  size.setHeight(size.height() - kBorder * 2);

  no_matches_label_->move(pos);
  no_matches_label_->resize(size);

}

void PlaylistContainer::SelectionChanged() {
  manager_->SelectionChanged(view()->selectionModel()->selection());
}

bool PlaylistContainer::eventFilter(QObject *objectWatched, QEvent *event) {

  if (objectWatched == ui_->search_field) {
    if (event->type() == QEvent::KeyPress) {
      QKeyEvent *e = static_cast<QKeyEvent*>(event);
      switch (e->key()) {
        //case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Return:
        case Qt::Key_Enter:
          view()->setFocus(Qt::OtherFocusReason);
          QApplication::sendEvent(ui_->playlist, event);
          return true;
        case Qt::Key_Escape:
          ui_->search_field->clear();
          return true;
        default:
          break;
      }
    }
  }
  return QWidget::eventFilter(objectWatched, event);

}
