/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef RADIOVIEW_H
#define RADIOVIEW_H

#include <QObject>

#include "widgets/autoexpandingtreeview.h"

class QMimeData;
class QMenu;
class QAction;
class QShowEvent;
class QContextMenuEvent;

class RadioModel;

class RadioView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  explicit RadioView(QWidget *parent = nullptr);
  ~RadioView();

  void showEvent(QShowEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

 Q_SIGNALS:
  void GetChannels();

 private Q_SLOTS:
  void AddToPlaylist();
  void ReplacePlaylist();
  void OpenInNewPlaylist();
  void Homepage();
  void Donate();

 private:
  QMenu *menu_;
  QAction *action_playlist_append_;
  QAction *action_playlist_replace_;
  QAction *action_playlist_new_;
  QAction *action_homepage_;
  QAction *action_donate_;
  bool initialized_;
};

#endif  // RADIOVIEW_H
