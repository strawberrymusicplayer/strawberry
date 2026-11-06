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

#include "gtest_include.h"

#include <QGuiApplication>
#include <QPoint>
#include <QSignalSpy>
#include <QStyleHints>
#include <QTest>

#include "widgets/longpresshandler.h"

TEST(LongPressHandlerTest, FiresOnceAndConsumesMoveAndRelease) {

  LongPressHandler handler(30, 10);
  QSignalSpy spy(&handler, &LongPressHandler::LongPressed);
  handler.Press(QPoint(20, 30));
  ASSERT_TRUE(spy.wait(1000));
  ASSERT_EQ(1, spy.count());
  EXPECT_EQ(QPoint(20, 30), spy.first().first().toPoint());
  EXPECT_TRUE(handler.Move(QPoint(100, 100)));
  EXPECT_TRUE(handler.Release());
  EXPECT_FALSE(handler.Release());
  QTest::qWait(60);
  EXPECT_EQ(1, spy.count());

}

TEST(LongPressHandlerTest, ShortTapDoesNotFireOrConsumeRelease) {

  LongPressHandler handler(30, 10);
  QSignalSpy spy(&handler, &LongPressHandler::LongPressed);
  handler.Press(QPoint(20, 30));
  EXPECT_FALSE(handler.Release());
  QTest::qWait(60);
  EXPECT_TRUE(spy.isEmpty());

}

TEST(LongPressHandlerTest, MovementBeyondTouchSlopCancelsPermanently) {

  LongPressHandler handler(30, 10);
  QSignalSpy spy(&handler, &LongPressHandler::LongPressed);
  handler.Press(QPoint(20, 30));
  EXPECT_FALSE(handler.Move(QPoint(31, 30)));
  EXPECT_FALSE(handler.Move(QPoint(20, 30)));
  QTest::qWait(60);
  EXPECT_TRUE(spy.isEmpty());
  EXPECT_FALSE(handler.Release());

}

TEST(LongPressHandlerTest, MovementWithinTouchSlopDoesNotCancel) {

  LongPressHandler handler(30, 10);
  QSignalSpy spy(&handler, &LongPressHandler::LongPressed);
  handler.Press(QPoint(20, 30));
  EXPECT_FALSE(handler.Move(QPoint(25, 35)));
  ASSERT_TRUE(spy.wait(1000));
  EXPECT_TRUE(handler.Release());

}

TEST(LongPressHandlerTest, CancelStopsPendingGesture) {

  LongPressHandler handler(30, 10);
  QSignalSpy spy(&handler, &LongPressHandler::LongPressed);
  handler.Press(QPoint(20, 30));
  handler.Cancel();
  QTest::qWait(60);
  EXPECT_TRUE(spy.isEmpty());
  EXPECT_FALSE(handler.Release());

}

TEST(LongPressHandlerTest, CancelAfterTriggerStillConsumesRelease) {

  LongPressHandler handler(30, 10);
  QSignalSpy spy(&handler, &LongPressHandler::LongPressed);
  handler.Press(QPoint(20, 30));
  ASSERT_TRUE(spy.wait(1000));
  handler.Cancel();
  EXPECT_TRUE(handler.Release());

}

TEST(LongPressHandlerTest, NewPressDoesNotInheritAnUndeliveredRelease) {

  LongPressHandler handler(30, 10);
  QSignalSpy spy(&handler, &LongPressHandler::LongPressed);
  handler.Press(QPoint(20, 30));
  ASSERT_TRUE(spy.wait(1000));
  handler.Press(QPoint(40, 50));
  EXPECT_FALSE(handler.Release());
  QTest::qWait(60);
  EXPECT_EQ(1, spy.count());

}

TEST(LongPressHandlerTest, UsesConfiguredTimeout) {

  LongPressHandler handler(150, 10);
  QSignalSpy spy(&handler, &LongPressHandler::LongPressed);
  handler.Press(QPoint(20, 30));
  QTest::qWait(50);
  EXPECT_TRUE(spy.isEmpty());
  ASSERT_TRUE(spy.wait(1000));
  EXPECT_TRUE(handler.Release());

}

TEST(LongPressHandlerTest, PlatformValuesFallBackToStyleHints) {

  EXPECT_EQ(QGuiApplication::styleHints()->mousePressAndHoldInterval(), LongPressHandler::PlatformInterval());
  EXPECT_EQ(QGuiApplication::styleHints()->startDragDistance(), LongPressHandler::PlatformDragDistance(2.0));

}
