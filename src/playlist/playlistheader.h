/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <stdbool.h>

#include <QObject>
#include <QWidget>
#include <QList>
#include <QtEvents>

#include "widgets/stretchheaderview.h"

class QMenu;
class QAction;

class PlaylistView;

class PlaylistHeader : public StretchHeaderView {
  Q_OBJECT

 public:
  PlaylistHeader(Qt::Orientation orientation, PlaylistView *view, QWidget *parent = nullptr);

  // QWidget
  void contextMenuEvent(QContextMenuEvent *e);
  void enterEvent(QEvent *);

 signals:
  void SectionVisibilityChanged(int logical, bool visible);
  void MouseEntered();

 private slots:
  void HideCurrent();
  void ToggleVisible(int section);
  void ResetColumns();
  void SetColumnAlignment(QAction *action);

 private:
  void AddColumnAction(int index);

 private:
  PlaylistView *view_;

  int menu_section_;
  QMenu *menu_;
  QAction *hide_action_;
  QAction *stretch_action_;
  QAction *reset_action_;
  QAction *align_left_action_;
  QAction *align_center_action_;
  QAction *align_right_action_;
  QList<QAction*> show_actions_;

};

#endif  // PLAYLISTHEADER_H

