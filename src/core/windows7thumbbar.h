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

#ifndef WINDOWS7THUMBBAR_H
#define WINDOWS7THUMBBAR_H

#include <windows.h>
#include <shobjidl.h>

#include <QObject>
#include <QWidget>
#include <QList>

class QTimer;
class QAction;

class Windows7ThumbBar : public QObject {
  Q_OBJECT

 public:
  // Creates a list of buttons in the taskbar icon for this window.
  explicit Windows7ThumbBar(QWidget *widget = nullptr);

  // You must call this in the parent widget's constructor before returning to the event loop.  If an action is nullptr it becomes a spacer.
  void SetActions(const QList<QAction*> &actions);

  // Call this from the parent's winEvent() function.
  void HandleWinEvent(MSG *msg);

 private:
  ITaskbarList3 *CreateTaskbarList();
  void SetupButton(const QAction *action, THUMBBUTTON *button);

 private Q_SLOTS:
  void ActionChangedTriggered();
  void ActionChanged();

 private:
  QWidget *widget_;
  QTimer *timer_;
  QList<QAction*> actions_;

  unsigned int button_created_message_id_;

  ITaskbarList3 *taskbar_list_;
};

#endif  // WINDOWS7THUMBBAR_H
