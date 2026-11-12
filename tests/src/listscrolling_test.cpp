// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QHeaderView>
#include <QListView>
#include <QMouseEvent>
#include <QScrollBar>
#include <QScroller>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStyleHints>
#include <QTest>
#include <QTreeView>
#include <QTableView>

#include "widgets/listscrolling_android.h"
#include "widgets/autoexpandingtreeview.h"
#include "widgets/playlistactionhandler.h"

namespace {

void Populate(QStandardItemModel *model) {
  for (int i = 0; i < 100; ++i) model->appendRow(new QStandardItem(QString::number(i)));
}

void Swipe(QWidget *viewport) {
  const QPoint start(100, 180);
  QTest::mousePress(viewport, Qt::LeftButton, Qt::NoModifier, start);
  for (int i = 1; i <= 8; ++i) {
    QTest::qWait(20);
    const QPoint position = start - QPoint(0, i * 18);
    QMouseEvent move(QEvent::MouseMove, position, viewport->mapToGlobal(position), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &move);
  }
  QTest::mouseRelease(viewport, Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 144));
  QTest::qWait(100);
}

}

TEST(AndroidListScrollingTest, AutomaticallyEnablesListsButNotHeadersOrComboPopups) {
  AndroidListScrolling scrolling(qApp);
  QStandardItemModel model;
  Populate(&model);
  AutoExpandingTreeView view;
  view.setModel(&model);
  view.setDragEnabled(true);
  view.resize(320, 240);
  view.show();
  QCoreApplication::processEvents();
  EXPECT_TRUE(QScroller::hasScroller(view.viewport()));
  EXPECT_FALSE(QScroller::hasScroller(view.header()->viewport()));
  EXPECT_FALSE(view.dragEnabled());
  EXPECT_EQ(view.verticalScrollMode(), QAbstractItemView::ScrollPerPixel);
  QComboBox combo;
  combo.addItems({QStringLiteral("One"), QStringLiteral("Two")});
  combo.show();
  combo.showPopup();
  QCoreApplication::processEvents();
  EXPECT_FALSE(QScroller::hasScroller(combo.view()->viewport()));
  combo.hidePopup();
}

TEST(AndroidListScrollingTest, SwipingScrollsWithoutClickingAndTapsStillWork) {
  AndroidListScrolling scrolling(qApp);
  QStandardItemModel model;
  Populate(&model);
  QListView view;
  view.setModel(&model);
  view.resize(320, 240);
  view.show();
  QCoreApplication::processEvents();
  QSignalSpy clicked(&view, &QAbstractItemView::clicked);
  Swipe(view.viewport());
  EXPECT_GT(view.verticalScrollBar()->value(), 0);
  EXPECT_EQ(clicked.count(), 0);
  EXPECT_EQ(QScroller::scroller(view.viewport())->state(), QScroller::Scrolling);
  QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 80));
  EXPECT_EQ(clicked.count(), 0);
  QScroller::scroller(view.viewport())->stop();
  QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 80));
  QTest::qWait(50);
  EXPECT_EQ(clicked.count(), 1);
}

TEST(AndroidListScrollingTest, SwipingCancelsAlbumLongPressButHoldingStillOpensActions) {
  AndroidListScrolling scrolling(qApp);
  QStandardItemModel model;
  Populate(&model);
  QListView view;
  view.setModel(&model);
  view.resize(320, 240);
  PlaylistActionHandler actions(&view, QStringLiteral("Collection"), {QStringLiteral("Add"), QStringLiteral("Queue"), QStringLiteral("Next")},
      [](const QModelIndex &) { return true; }, [](const QModelIndex &) -> MimeData* { return nullptr; });
  view.show();
  QCoreApplication::processEvents();
  Swipe(view.viewport());
  QTest::qWait(QApplication::styleHints()->mousePressAndHoldInterval() + 100);
  for (QDialog *dialog : view.findChildren<QDialog*>()) EXPECT_FALSE(dialog->isVisible());
  QScroller::scroller(view.viewport())->stop();
  QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 80));
  QTest::qWait(QApplication::styleHints()->mousePressAndHoldInterval() + 100);
  bool visible = false;
  for (QDialog *dialog : view.findChildren<QDialog*>()) {
    if (dialog->isVisible()) {
      visible = true;
      dialog->reject();
    }
  }
  EXPECT_TRUE(visible);
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 80));
}

TEST(AndroidListScrollingTest, TreeSwipeDoesNotExpandOrToggleItems) {
  AndroidListScrolling scrolling(qApp);
  QStandardItemModel model;
  for (int i = 0; i < 80; ++i) {
    auto *item = new QStandardItem(QString::number(i));
    item->setCheckable(true);
    item->appendRow(new QStandardItem(QStringLiteral("Child")));
    model.appendRow(item);
  }
  AutoExpandingTreeView view;
  view.setModel(&model);
  view.resize(320, 240);
  view.show();
  QCoreApplication::processEvents();
  QSignalSpy expanded(&view, &QTreeView::expanded);
  QSignalSpy changed(&model, &QStandardItemModel::itemChanged);
  Swipe(view.viewport());
  EXPECT_GT(view.verticalScrollBar()->value(), 0);
  EXPECT_EQ(expanded.count(), 0);
  EXPECT_EQ(changed.count(), 0);
}

TEST(AndroidListScrollingTest, HorizontalSwipeScrollsWideTables) {
  AndroidListScrolling scrolling(qApp);
  QStandardItemModel model(50, 20);
  QTableView view;
  view.setModel(&model);
  view.resize(320, 240);
  view.show();
  QCoreApplication::processEvents();
  QSignalSpy clicked(&view, &QAbstractItemView::clicked);
  const QPoint start(240, 100);
  QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, start);
  for (int i = 1; i <= 8; ++i) {
    QTest::qWait(20);
    const QPoint position = start - QPoint(i * 20, 0);
    QMouseEvent move(QEvent::MouseMove, position, view.viewport()->mapToGlobal(position), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &move);
  }
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(80, 100));
  QTest::qWait(100);
  EXPECT_GT(view.horizontalScrollBar()->value(), 0);
  EXPECT_EQ(clicked.count(), 0);
  EXPECT_FALSE(QScroller::hasScroller(view.horizontalHeader()->viewport()));
}
