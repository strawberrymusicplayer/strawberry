/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "gtest_include.h"

#include <QApplication>
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QPoint>
#include <QString>
#include <QSignalSpy>
#include <QContextMenuEvent>

#include "radios/radioview.h"

using namespace Qt::Literals::StringLiterals;

namespace {

QAction *FindAction(const QMenu *menu, const QString &text) {

  const QList<QAction*> actions = menu->actions();
  for (QAction *action : actions) {
    if (action->text() == text) return action;
  }

  return nullptr;

}

// The context menu of the view has no parent, so it's one of the top level widgets.
QMenu *FindMenuWithAction(const QString &text) {

  const QWidgetList top_level_widgets = QApplication::topLevelWidgets();
  for (QWidget *widget : top_level_widgets) {
    QMenu *menu = qobject_cast<QMenu*>(widget);
    if (menu && FindAction(menu, text)) return menu;
  }

  return nullptr;

}

// Regression test: "Refresh channels" in the context menu only loaded the channels from the database again, not from the radio services, like the refresh button.
TEST(RadioViewTest, RefreshChannelsInMenuRefreshesChannels) {

  RadioView radio_view;
  QSignalSpy get_channels_spy(&radio_view, &RadioView::GetChannels);
  QSignalSpy refresh_channels_spy(&radio_view, &RadioView::RefreshChannels);

  QContextMenuEvent context_menu_event(QContextMenuEvent::Mouse, QPoint(1, 1), QPoint(1, 1));
  // Scroll areas only handle mouse context menu events sent to the viewport.
  QApplication::sendEvent(radio_view.viewport(), &context_menu_event);

  QMenu *menu = FindMenuWithAction(u"Refresh channels"_s);
  ASSERT_NE(nullptr, menu);
  FindAction(menu, u"Refresh channels"_s)->trigger();
  menu->close();

  EXPECT_EQ(1, refresh_channels_spy.count());
  EXPECT_EQ(0, get_channels_spy.count());

}

}  // namespace
