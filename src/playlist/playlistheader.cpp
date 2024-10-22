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

#include "playlistheader.h"
#include "playlistview.h"

#include "core/settings.h"
#include "widgets/stretchheaderview.h"
#include "constants/playlistsettings.h"

PlaylistHeader::PlaylistHeader(Qt::Orientation orientation, PlaylistView *view, QWidget *parent)
    : StretchHeaderView(orientation, parent),
      view_(view),
      menu_section_(0),
      menu_(new QMenu(this)),
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
  action_rating_lock_->setChecked(s.value(PlaylistSettings::kRatingLocked, false).toBool());
  s.endGroup();

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

  qDeleteAll(show_actions_);
  show_actions_.clear();
  for (int i = 0; i < count(); ++i) {
    AddColumnAction(i);
  }

  menu_->popup(e->globalPos());

}

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
