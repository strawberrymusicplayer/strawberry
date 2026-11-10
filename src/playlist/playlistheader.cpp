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

#include <QWidget>
#include <QFlags>
#include <QVariant>
#include <QString>
#include <QtAlgorithms>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QEvent>
#include <QContextMenuEvent>
#include <QEnterEvent>
#ifdef Q_OS_ANDROID
#include <QApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#endif

#include "playlistheader.h"
#include "playlistview.h"

#include "core/settings.h"
#include "widgets/stretchheaderview.h"
#include "constants/playlistsettings.h"
#ifdef Q_OS_ANDROID
#include "widgets/actiondialog.h"
#include "widgets/longpresshandler.h"
#endif

PlaylistHeader::PlaylistHeader(Qt::Orientation orientation, PlaylistView *view, QWidget *parent)
    : StretchHeaderView(orientation, parent),
      view_(view),
      menu_section_(0),
      menu_(new QMenu(this)),
      sort_menu_(nullptr),
      action_sort_ascending_(nullptr),
      action_sort_descending_(nullptr),
      action_sort_clear_(nullptr),
      action_hide_(nullptr),
      action_reset_(nullptr),
      action_stretch_(nullptr),
      action_rating_lock_(nullptr),
      action_align_left_(nullptr),
      action_align_center_(nullptr),
      action_align_right_(nullptr) {

  action_hide_ = menu_->addAction(tr("&Hide..."), this, &PlaylistHeader::HideCurrent);
  action_stretch_ = menu_->addAction(tr("&Stretch columns to fit window"), this, &PlaylistHeader::ToggleStretchEnabled);
  action_reset_ = menu_->addAction(tr("&Reset columns to default"), this, &PlaylistHeader::ResetColumns);
  action_rating_lock_ = menu_->addAction(tr("&Lock rating"), this, &PlaylistHeader::ToggleRatingEditStatus);
  action_rating_lock_->setCheckable(true);
  menu_->addSeparator();

  // Sorting is normally done by clicking the header, so this covers what clicking cannot express: picking an order directly instead of cycling through it, and clearing the sorting again.
  sort_menu_ = new QMenu(tr("Sor&t"), this);
  action_sort_ascending_ = sort_menu_->addAction(tr("&Ascending"), this, &PlaylistHeader::SortAscending);
  action_sort_descending_ = sort_menu_->addAction(tr("&Descending"), this, &PlaylistHeader::SortDescending);
  action_sort_ascending_->setCheckable(true);
  action_sort_descending_->setCheckable(true);
  sort_menu_->addSeparator();
  action_sort_clear_ = sort_menu_->addAction(tr("&Clear sorting"), this, &PlaylistHeader::ClearSorting);
  menu_->addMenu(sort_menu_);

  QMenu *align_menu = new QMenu(tr("&Align text"), this);
  QActionGroup *align_group = new QActionGroup(this);
  action_align_left_ = new QAction(tr("&Left"), align_group);
  action_align_center_ = new QAction(tr("&Center"), align_group);
  action_align_right_ = new QAction(tr("&Right"), align_group);

  action_align_left_->setCheckable(true);
  action_align_center_->setCheckable(true);
  action_align_right_->setCheckable(true);
  align_menu->addActions(align_group->actions());

  QObject::connect(align_group, &QActionGroup::triggered, this, &PlaylistHeader::SetColumnAlignment);

  menu_->addMenu(align_menu);
  menu_->addSeparator();

  action_stretch_->setCheckable(true);
  action_stretch_->setChecked(is_stretch_enabled());

  QObject::connect(this, &PlaylistHeader::StretchEnabledChanged, action_stretch_, &QAction::setChecked);

  Settings s;
  s.beginGroup(PlaylistSettings::kSettingsGroup);
  action_rating_lock_->setChecked(s.value(PlaylistSettings::kRatingLocked, PlaylistSettings::kDefaultRatingLocked).toBool());
  s.endGroup();

#ifdef Q_OS_ANDROID
  long_press_ = new LongPressHandler(LongPressHandler::PlatformInterval(), LongPressHandler::PlatformDragDistance(devicePixelRatioF()), this);
  setContextMenuPolicy(Qt::PreventContextMenu);
  QObject::connect(long_press_, &LongPressHandler::LongPressed, this, &PlaylistHeader::ShowAndroidActions);
  QObject::connect(qApp, &QApplication::applicationStateChanged, this, [this](const Qt::ApplicationState state) {
    if (state != Qt::ApplicationActive) CancelAndroidActions();
  });
#endif

}

void PlaylistHeader::contextMenuEvent(QContextMenuEvent *e) {

  menu_section_ = logicalIndexAt(e->pos());

  if (menu_section_ == -1 || (menu_section_ == logicalIndex(0) && logicalIndex(1) == -1)) {
    action_hide_->setVisible(false);
  }
  else {
    action_hide_->setVisible(true);

    QString title(model()->headerData(menu_section_, Qt::Horizontal).toString());
    action_hide_->setText(tr("&Hide %1").arg(title));

    Qt::Alignment alignment = view_->column_alignment(menu_section_);
    if      (alignment & Qt::AlignLeft)    action_align_left_->setChecked(true);
    else if (alignment & Qt::AlignHCenter) action_align_center_->setChecked(true);
    else if (alignment & Qt::AlignRight)   action_align_right_->setChecked(true);

    // Show rating lock action only for ratings section
    action_rating_lock_->setVisible(menu_section_ == static_cast<int>(Playlist::Column::Rating));

  }

  // Ascending/descending act on the section that was clicked, so they are only available when one was; clearing applies to the playlist as a whole.
  const bool has_section = menu_section_ != -1;
  const bool section_is_sorted = has_section && sortIndicatorSection() == menu_section_;
  action_sort_ascending_->setEnabled(has_section);
  action_sort_descending_->setEnabled(has_section);
  action_sort_ascending_->setChecked(section_is_sorted && sortIndicatorOrder() == Qt::AscendingOrder);
  action_sort_descending_->setChecked(section_is_sorted && sortIndicatorOrder() == Qt::DescendingOrder);
  action_sort_clear_->setEnabled(sortIndicatorSection() != -1);

  qDeleteAll(show_actions_);
  show_actions_.clear();
  for (int i = 0; i < count(); ++i) {
    AddColumnAction(i);
  }

  menu_->popup(e->globalPos());

}

#ifdef Q_OS_ANDROID
void PlaylistHeader::mousePressEvent(QMouseEvent *event) {

  long_press_->Release();
  if (event->button() == Qt::LeftButton && logicalIndexAt(event->pos()) != -1) {
    long_press_->Press(event->pos());
  }
  StretchHeaderView::mousePressEvent(event);

}

void PlaylistHeader::mouseMoveEvent(QMouseEvent *event) {

  if (long_press_->Move(event->pos())) {
    event->accept();
    return;
  }
  StretchHeaderView::mouseMoveEvent(event);

}

void PlaylistHeader::mouseReleaseEvent(QMouseEvent *event) {

  if (long_press_->Release()) {
    event->accept();
    return;
  }
  StretchHeaderView::mouseReleaseEvent(event);

}

void PlaylistHeader::CancelAndroidActions() {

  long_press_->Cancel();
  if (android_actions_ && android_actions_->isVisible()) android_actions_->reject();

}

void PlaylistHeader::ShowAndroidActions(const QPoint position) {

  const int section = logicalIndexAt(position);
  if (section == -1 || !isVisible()) return;

  android_actions_ = CreateAndroidActions(this, section);
  android_actions_->Open();

}

ActionDialog *PlaylistHeader::CreateAndroidActions(QWidget *parent, const int section) {

  menu_section_ = section;
  const QString title = model()->headerData(section, Qt::Horizontal).toString();
  ActionDialog *dialog = new ActionDialog(parent, tr("Playlist columns and sorting"), tr("Playlist > %1").arg(title), ActionDialog::TitlePosition::Top);
  const auto add_action = [dialog](const QString &text, const auto action) {
    QPushButton *button = dialog->AddButton(text);
    QObject::connect(button, &QPushButton::clicked, dialog, [dialog, action]() {
      dialog->accept();
      action();
    });
  };

  add_action(tr("Sort by %1 ascending").arg(title), [this]() { SortAscending(); });
  add_action(tr("Sort by %1 descending").arg(title), [this]() { SortDescending(); });
  if (sortIndicatorSection() != -1) {
    add_action(tr("Clear sorting"), [this]() { ClearSorting(); });
  }

  int visible_columns = 0;
  for (int i = 0; i < count(); ++i) {
    if (!isSectionHidden(i)) ++visible_columns;
  }
  if (visible_columns > 1) {
    add_action(tr("Hide %1").arg(title), [this, section]() { ToggleVisible(section); });
  }

  const bool portrait = screen() && screen()->geometry().height() > screen()->geometry().width();
  if (!portrait) {
    add_action(is_stretch_enabled() ? tr("Stop stretching columns") : tr("Stretch columns to fit"), [this]() { ToggleStretchEnabled(); });
  }
  add_action(tr("Reset columns to default"), [this]() { ResetColumns(); });
  dialog->AddCancelButton(tr("Back"));
  return dialog;

}
#endif

void PlaylistHeader::AddColumnAction(const int index) {

#ifndef HAVE_MOODBAR
  if (index == static_cast<int>(Playlist::Column::Mood)) {
    return;
  }
#endif

  QString title(model()->headerData(index, Qt::Horizontal).toString());

  QAction *action = menu_->addAction(title);
  action->setCheckable(true);
  action->setChecked(!isSectionHidden(index));
  show_actions_ << action;

  QObject::connect(action, &QAction::triggered, this, [this, index]() { ToggleVisible(index); });

}

void PlaylistHeader::SortAscending() {
  SortCurrent(Qt::AscendingOrder);
}

void PlaylistHeader::SortDescending() {
  SortCurrent(Qt::DescendingOrder);
}

void PlaylistHeader::SortCurrent(const Qt::SortOrder sort_order) {

  if (menu_section_ == -1) return;

  // Already sorted this way, so do nothing rather than sort the playlist again and add another entry to the undo stack for an order it is already in.
  if (sortIndicatorSection() == menu_section_ && sortIndicatorOrder() == sort_order) return;

  // The view sorts through the header's sort indicator (sorting is enabled on it), so setting the indicator is what actually sorts the playlist.
  setSortIndicator(menu_section_, sort_order);

}

void PlaylistHeader::ClearSorting() {

  if (sortIndicatorSection() == -1) return;

  // Clearing the indicator makes Playlist::sort() stop treating the playlist as sorted, leaving the items in their current order.
  setSortIndicator(-1, sortIndicatorOrder());

}

void PlaylistHeader::HideCurrent() {
  if (menu_section_ == -1) return;

  SetSectionHidden(menu_section_, true);
}

void PlaylistHeader::SetColumnAlignment(QAction *action) {

  Qt::Alignment alignment = Qt::AlignVCenter;

  if (action == action_align_left_) alignment |= Qt::AlignLeft;
  if (action == action_align_center_) alignment |= Qt::AlignHCenter;
  if (action == action_align_right_) alignment |= Qt::AlignRight;

  view_->SetColumnAlignment(menu_section_, alignment);

}

void PlaylistHeader::ToggleVisible(const int section) {
  SetSectionHidden(section, !isSectionHidden(section));
  Q_EMIT SectionVisibilityChanged(section, !isSectionHidden(section));
}

void PlaylistHeader::enterEvent(QEnterEvent *e) {
  Q_UNUSED(e)
  Q_EMIT MouseEntered();
}

void PlaylistHeader::ResetColumns() {
  view_->ResetHeaderState();
}

void PlaylistHeader::ToggleRatingEditStatus() {
  Q_EMIT SectionRatingLockStatusChanged(action_rating_lock_->isChecked());
}
