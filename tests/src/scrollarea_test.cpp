// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QSignalSpy>
#include <QTest>
#include <QTextEdit>
#include <QVBoxLayout>

#include "widgets/scrollarea_android.h"

namespace {

void Swipe(QWidget *target) {
  const QPoint start(100, 180);
  QTest::mousePress(target, Qt::LeftButton, Qt::NoModifier, start);
  for (int i = 1; i <= 8; ++i) {
    QTest::qWait(20);
    const QPoint position = start - QPoint(0, i * 18);
    QMouseEvent move(QEvent::MouseMove, position, target->mapToGlobal(position), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
  }
  QTest::mouseRelease(target, Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 144));
  QTest::qWait(100);
}

}

TEST(AndroidScrollAreaTest, SwipingTextAndContentScrollsOuterArea) {
  QScrollArea area;
  area.setWidgetResizable(true);
  QWidget *content = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(content);
  QTextEdit *text = new QTextEdit;
  text->setReadOnly(true);
  text->setFixedHeight(400);
  layout->addWidget(text);
  QLabel *label = new QLabel(QStringLiteral("Below"));
  label->setFixedHeight(400);
  layout->addWidget(label);
  area.setWidget(content);
  AndroidScrollArea scrolling(&area);
  scrolling.AddGestureTarget(content);
  scrolling.AddGestureTarget(text->viewport());
  area.resize(320, 240);
  area.show();
  QCoreApplication::processEvents();

  Swipe(text->viewport());
  EXPECT_GT(area.verticalScrollBar()->value(), 0);
  QScroller::scroller(text->viewport())->stop();
  area.verticalScrollBar()->setValue(0);
  Swipe(content);
  EXPECT_GT(area.verticalScrollBar()->value(), 0);
}

TEST(AndroidScrollAreaTest, TapActivatesTargetButSwipeDoesNot) {
  QScrollArea area;
  area.setWidgetResizable(true);
  QWidget *content = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(content);
  QPushButton *button = new QPushButton(QStringLiteral("Action"));
  button->setFixedHeight(400);
  layout->addWidget(button);
  layout->addSpacing(400);
  area.setWidget(content);
  AndroidScrollArea scrolling(&area);
  scrolling.AddGestureTarget(button);
  area.resize(320, 240);
  area.show();
  QCoreApplication::processEvents();
  QSignalSpy clicked(button, &QPushButton::clicked);

  Swipe(button);
  EXPECT_GT(area.verticalScrollBar()->value(), 0);
  EXPECT_EQ(clicked.count(), 0);
  QScroller::scroller(button)->stop();
  area.verticalScrollBar()->setValue(0);
  QTest::mouseClick(button, Qt::LeftButton, Qt::NoModifier, QPoint(100, 80));
  EXPECT_EQ(clicked.count(), 1);
}

TEST(AndroidScrollAreaTest, DestroyedGestureTargetDoesNotLeaveDanglingScroller) {
  QScrollArea area;
  QWidget *content = new QWidget;
  content->resize(320, 800);
  QWidget *first = new QWidget(content);
  QWidget *second = new QWidget(content);
  first->setGeometry(0, 0, 300, 240);
  second->setGeometry(0, 300, 300, 240);
  area.setWidget(content);
  AndroidScrollArea scrolling(&area);
  scrolling.AddGestureTarget(first);
  scrolling.AddGestureTarget(second);
  delete first;
  area.resize(320, 240);
  area.show();
  QCoreApplication::processEvents();
  Swipe(second);
  EXPECT_GT(area.verticalScrollBar()->value(), 0);
}
