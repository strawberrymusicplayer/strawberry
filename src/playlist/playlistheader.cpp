/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QAbstractItemModel>
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

#include "settings/playlistsettingspage.h"

PlaylistHeader::PlaylistHeader(Qt::Orientation orientation, PlaylistView *view, QWidget *parent)
    : StretchHeaderView(orientation, parent),
      view_(view),
      menu_(new QMenu(this)),
      action_hide_(nullptr),
      action_reset_(nullptr),
      action_stretch_(nullptr),
      action_rating_lock_(nullptr),
      action_align_left_(nullptr),
      action_align_center_(nullptr),
      action_align_right_(nullptr)
      {

  action_hide_ = menu_->addAction(tr("&Hide..."), this, SLOT(HideCurrent()));
  action_stretch_ = menu_->addAction(tr("&Stretch columns to fit window"), this, SLOT(ToggleStretchEnabled()));
  action_reset_ = menu_->addAction(tr("&Reset columns to default"), this, SLOT(ResetColumns()));
  action_rating_lock_ = menu_->addAction(tr("&Lock rating"), this, SLOT(ToggleRatingEditStatus()));
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

  connect(align_group, SIGNAL(triggered(QAction*)), SLOT(SetColumnAlignment(QAction*)));

  menu_->addMenu(align_menu);
  menu_->addSeparator();

  action_stretch_->setCheckable(true);
  action_stretch_->setChecked(is_stretch_enabled());

  connect(this, SIGNAL(StretchEnabledChanged(bool)), action_stretch_, SLOT(setChecked(bool)));

  QSettings s;
  s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
  action_rating_lock_->setChecked(s.value("rating_locked", false).toBool());
  s.endGroup();

}

void PlaylistHeader::contextMenuEvent(QContextMenuEvent *e) {

  menu_section_ = logicalIndexAt(e->pos());

  if (menu_section_ == -1 || (menu_section_ == logicalIndex(0) && logicalIndex(1) == -1))
    action_hide_->setVisible(false);
  else {
    action_hide_->setVisible(true);

    QString title(model()->headerData(menu_section_, Qt::Horizontal).toString());
    action_hide_->setText(tr("&Hide %1").arg(title));

    Qt::Alignment alignment = view_->column_alignment(menu_section_);
    if      (alignment & Qt::AlignLeft)    action_align_left_->setChecked(true);
    else if (alignment & Qt::AlignHCenter) action_align_center_->setChecked(true);
    else if (alignment & Qt::AlignRight)   action_align_right_->setChecked(true);

    // Show rating lock action only for ratings section
    action_rating_lock_->setVisible(menu_section_ == Playlist::Column_Rating);

  }

  qDeleteAll(show_actions_);
  show_actions_.clear();
  for (int i = 0 ; i < count() ; ++i) {
    AddColumnAction(i);
  }

  menu_->popup(e->globalPos());

}

void PlaylistHeader::AddColumnAction(int index) {

#ifndef HAVE_MOODBAR
  if (index == Playlist::Column_Mood) {
    return;
  }
#endif

  QString title(model()->headerData(index, Qt::Horizontal).toString());

  QAction *action = menu_->addAction(title);
  action->setCheckable(true);
  action->setChecked(!isSectionHidden(index));
  show_actions_ << action;

  connect(action, &QAction::triggered, [this, index]() { ToggleVisible(index); } );

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

void PlaylistHeader::ToggleVisible(int section) {
  SetSectionHidden(section, !isSectionHidden(section));
  emit SectionVisibilityChanged(section, !isSectionHidden(section));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void PlaylistHeader::enterEvent(QEnterEvent*) {
#else
void PlaylistHeader::enterEvent(QEvent*) {
#endif
  emit MouseEntered();
}

void PlaylistHeader::ResetColumns() {
  view_->ResetHeaderState();
}

void PlaylistHeader::ToggleRatingEditStatus() {
  emit SectionRatingLockStatusChanged(action_rating_lock_->isChecked());
}

