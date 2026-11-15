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
#include <QSlider>
#include <QTest>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include "widgets/scrollarea_android.h"

namespace {

// Like a finger, keeps the screen positions fixed while the widget under it moves with the scrolled contents.
void Swipe(QWidget *target, const QPoint &start_position, const QPoint &distance) {
  const QPoint start = target->mapToGlobal(start_position);
  const auto local = [target](const QPoint &global) { return target->mapFromGlobal(global); };
  QTest::mousePress(target, Qt::LeftButton, Qt::NoModifier, local(start));
  const int steps = 12;
  for (int i = 1; i <= steps; ++i) {
    QTest::qWait(20);
    const QPoint global = start + distance * i / steps;
    QMouseEvent move(QEvent::MouseMove, local(global), global, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
  }
  // Hold still before releasing, so the release does not start a kinetic scroll.
  QTest::qWait(300);
  const QPoint end = start + distance;
  QMouseEvent release(QEvent::MouseButtonRelease, local(end), end, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(target, &release);
  QTest::qWait(100);
}

void SwipeUp(QWidget *target, const int distance = 144) {
  Swipe(target, QPoint(100, 180), QPoint(0, -distance));
}

}  // namespace

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
  AndroidScrollArea::EnableForContents(&area);
  area.resize(320, 240);
  area.show();
  QCoreApplication::processEvents();

  SwipeUp(text->viewport());
  EXPECT_GT(area.verticalScrollBar()->value(), 0);
  QScroller::scroller(area.viewport())->stop();
  area.verticalScrollBar()->setValue(0);
  SwipeUp(content);
  EXPECT_GT(area.verticalScrollBar()->value(), 0);
}

TEST(AndroidScrollAreaTest, EnableForContentsScrollsFromButtonsWithoutActivatingThem) {
  QScrollArea area;
  area.setWidgetResizable(true);
  QWidget *content = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(content);
  area.setWidget(content);
  AndroidScrollArea::EnableForContents(&area);
  // Buttons added after enabling scrolling work the same.
  QList<QToolButton*> buttons;
  for (int i = 0; i < 8; ++i) {
    QToolButton *button = new QToolButton;
    button->setText(QStringLiteral("Tool %1").arg(i));
    button->setFixedSize(300, 72);
    layout->addWidget(button);
    buttons << button;
  }
  area.resize(320, 240);
  area.show();
  QCoreApplication::processEvents();
  QSignalSpy clicked(buttons.at(1), &QToolButton::clicked);

  Swipe(buttons.at(1), QPoint(150, 36), QPoint(0, -144));
  EXPECT_GT(area.verticalScrollBar()->value(), 0);
  EXPECT_EQ(clicked.count(), 0);
  QScroller::scroller(area.viewport())->stop();
  area.verticalScrollBar()->setValue(0);
  // Tap later than the double-click interval, as a quick second press would be a double click.
  QTest::qWait(QApplication::doubleClickInterval() + 50);
  QTest::mouseClick(buttons.at(1), Qt::LeftButton);
  EXPECT_EQ(clicked.count(), 1);
}

TEST(AndroidScrollAreaTest, ContentsFollowTheFinger) {
  QScrollArea area;
  area.setWidgetResizable(true);
  QWidget *content = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(content);
  QPushButton *button = new QPushButton(QStringLiteral("Action"));
  button->setFixedHeight(400);
  layout->addWidget(button);
  layout->addSpacing(800);
  area.setWidget(content);
  AndroidScrollArea::EnableForContents(&area);
  area.resize(320, 240);
  area.show();
  QCoreApplication::processEvents();

  // The contents and the button move with the scrolled area, the finger does not.
  for (QWidget *target : {static_cast<QWidget*>(button), content}) {
    area.verticalScrollBar()->setValue(0);
    const int distance = 200;
    SwipeUp(target, distance);
    // Allow for the distance needed to start dragging.
    EXPECT_GE(area.verticalScrollBar()->value(), distance - 40) << target->metaObject()->className();
    QScroller::scroller(area.viewport())->stop();
  }
}

TEST(AndroidScrollAreaTest, DraggingSliderAdjustsItInsteadOfScrolling) {
  QScrollArea area;
  area.setWidgetResizable(true);
  QWidget *content = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(content);
  QSlider *slider = new QSlider(Qt::Horizontal);
  slider->setRange(0, 100);
  slider->setFixedHeight(48);
  layout->addWidget(slider);
  layout->addSpacing(800);
  area.setWidget(content);
  AndroidScrollArea::EnableForContents(&area);
  area.resize(320, 240);
  area.show();
  QCoreApplication::processEvents();

  const QPoint handle(slider->width() / 2, slider->height() / 2);
  slider->setValue(50);
  Swipe(slider, handle, QPoint(-60, -100));
  EXPECT_EQ(area.verticalScrollBar()->value(), 0);
  EXPECT_LT(slider->value(), 50);
}
