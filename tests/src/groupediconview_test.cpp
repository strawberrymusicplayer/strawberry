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

#include <utility>

#include "gtest_include.h"

#include <QList>
#include <QTest>
#include <QString>
#include <QRect>
#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QAbstractItemView>
#include <QStandardItem>
#include <QStandardItemModel>

#include "widgets/groupediconview.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Adds the items with the groups to the model.
void AddItems(QStandardItemModel *model, const QList<std::pair<QString, QString>> &items) {
  for (const std::pair<QString, QString> &item : items) {
    QStandardItem *standard_item = new QStandardItem(item.first);
    standard_item->setData(item.second, GroupedIconView::Role_Group);
    model->appendRow(standard_item);
  }
}

// Regression test: the positions of the items and the headers were not updated when rows were removed, so the items were shown under the wrong headers.
TEST(GroupedIconViewTest, LayoutUpdatedWhenRowsRemoved) {

  QStandardItemModel model;
  AddItems(&model, {{u"Removed"_s, u"A"_s}, {u"Kept A"_s, u"A"_s}, {u"Kept B"_s, u"B"_s}});
  GroupedIconView view;
  view.resize(400, 400);
  view.setModel(&model);
  // The list view only updates the layout later when it's shown.
  view.show();
  ASSERT_TRUE(QTest::qWaitForWindowExposed(&view));

  model.removeRow(0);
  // The list view updates the size of the items later.
  QTest::qWait(50);

  // The positions are the same as in a view with only the items which are left.
  QStandardItemModel expected_model;
  AddItems(&expected_model, {{u"Kept A"_s, u"A"_s}, {u"Kept B"_s, u"B"_s}});
  GroupedIconView expected_view;
  expected_view.resize(400, 400);
  expected_view.setModel(&expected_model);

  ASSERT_EQ(2, view.model()->rowCount());
  for (int row = 0; row < 2; ++row) {
    SCOPED_TRACE(row);
    EXPECT_EQ(view.model()->index(row, 0).data().toString(), expected_view.model()->index(row, 0).data().toString());
    // The position is protected in the view, but public in the base class.
    const QAbstractItemView *base_view = &view;
    const QAbstractItemView *expected_base_view = &expected_view;
    EXPECT_EQ(expected_base_view->visualRect(expected_view.model()->index(row, 0)), base_view->visualRect(view.model()->index(row, 0)));
  }

}

// Counts the headers which are drawn, the height of each header is used once when it's drawn.
class HeaderCountingView : public GroupedIconView {
 public:
  explicit HeaderCountingView(QWidget *parent = nullptr) : GroupedIconView(parent), header_height_count_(0) {}
  int header_height() const override {
    ++header_height_count_;
    return GroupedIconView::header_height();
  }
  // Returns the number of headers drawn when the view is drawn.
  int DrawnHeaderCount() {
    header_height_count_ = 0;
    const QImage image = grab().toImage();
    Q_UNUSED(image)
    return header_height_count_;
  }

 private:
  mutable int header_height_count_;
};

// Regression test: the positions were updated again while they were being updated, when getting the size of an item updated the pending layout of the list view, like after rows were inserted.
// The update which started first then added all items and headers again, so the headers were drawn twice.
TEST(GroupedIconViewTest, HeadersDrawnOnceWhenRowsInserted) {

  QStandardItemModel model;
  AddItems(&model, {{u"A1"_s, u"A"_s}});
  HeaderCountingView view;
  view.resize(400, 400);
  view.setModel(&model);
  view.show();
  ASSERT_TRUE(QTest::qWaitForWindowExposed(&view));
  EXPECT_EQ(1, view.DrawnHeaderCount());

  AddItems(&model, {{u"A2"_s, u"A"_s}, {u"B1"_s, u"B"_s}});
  QTest::qWait(50);
  EXPECT_EQ(2, view.DrawnHeaderCount());

}

}  // namespace
