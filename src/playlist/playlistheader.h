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

#ifndef PLAYLISTHEADER_H
#define PLAYLISTHEADER_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QList>

#include "widgets/stretchheaderview.h"

class QMenu;
class QAction;
class QContextMenuEvent;
class QEnterEvent;

class PlaylistView;

class PlaylistHeader : public StretchHeaderView {
  Q_OBJECT

 public:
  explicit PlaylistHeader(Qt::Orientation orientation, PlaylistView *view, QWidget *parent = nullptr);

  // QWidget
  void contextMenuEvent(QContextMenuEvent *e) override;
  void enterEvent(QEnterEvent*) override;

 Q_SIGNALS:
  void SectionVisibilityChanged(const int logical_index, const bool visible);
  void MouseEntered();
  void SectionRatingLockStatusChanged(const bool);

 private Q_SLOTS:
  void HideCurrent();
  void ToggleVisible(const int section);
  void ResetColumns();
  void SetColumnAlignment(QAction *action);
  void ToggleRatingEditStatus();

 private:
  void AddColumnAction(const int index);

 private:
  PlaylistView *view_;

  int menu_section_;
  QMenu *menu_;
  QAction *action_hide_;
  QAction *action_reset_;
  QAction *action_stretch_;
  QAction *action_rating_lock_;
  QAction *action_align_left_;
  QAction *action_align_center_;
  QAction *action_align_right_;
  QList<QAction*> show_actions_;

};

#endif  // PLAYLISTHEADER_H
