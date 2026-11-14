/*
 * Strawberry Music Player
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

#include <utility>

#include <QCoreApplication>
#include <QList>
#include <QRect>
#include <QSizePolicy>
#include <QToolButton>
#include <QWidget>

#include "widgets/gridflowlayout.h"

namespace {

class GridFlowLayoutTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (int i = 0; i < 6; ++i) {
      QWidget *child = new QWidget(&container_);
      child->setMinimumSize(i == 1 ? 60 : 50, i == 2 ? 30 : 20);
      children_.append(child);
    }
  }

  GridFlowLayout *CreateLayout(const int spacing, const int maximum_columns) {
    GridFlowLayout *layout = new GridFlowLayout(spacing, maximum_columns);
    container_.setLayout(layout);
    for (QWidget *child : std::as_const(children_)) layout->addWidget(child);
    return layout;
  }

  QWidget container_;
  QList<QWidget*> children_;
};

}  // namespace

TEST_F(GridFlowLayoutTest, UsesTheLargestItemAsCell) {

  GridFlowLayout *layout = CreateLayout(0, 0);
  EXPECT_TRUE(layout->hasHeightForWidth());
  EXPECT_EQ(QSize(60, 30), layout->minimumSize());
  EXPECT_EQ(Qt::Horizontal, layout->expandingDirections());

}

TEST_F(GridFlowLayoutTest, WrapsToTheColumnsThatFit) {

  GridFlowLayout *layout = CreateLayout(4, 0);
  EXPECT_EQ(1, layout->ColumnsForWidth(10));
  EXPECT_EQ(1, layout->ColumnsForWidth(123));
  EXPECT_EQ(2, layout->ColumnsForWidth(124));
  EXPECT_EQ(3, layout->ColumnsForWidth(188));
  EXPECT_EQ(6, layout->ColumnsForWidth(1000));
  EXPECT_EQ(6 * 30 + 5 * 4, layout->heightForWidth(60));
  EXPECT_EQ(2 * 30 + 4, layout->heightForWidth(188));
  EXPECT_EQ(30, layout->heightForWidth(1000));

}

TEST_F(GridFlowLayoutTest, LimitsTheColumns) {

  GridFlowLayout *layout = CreateLayout(4, 4);
  EXPECT_EQ(4, layout->maximum_columns());
  EXPECT_EQ(4, layout->ColumnsForWidth(1000));
  EXPECT_EQ(2 * 30 + 4, layout->heightForWidth(1000));

}

TEST_F(GridFlowLayoutTest, StretchesCellsToFillEachRow) {

  GridFlowLayout *layout = CreateLayout(4, 0);
  layout->setGeometry(QRect(10, 20, 200, 64));
  EXPECT_EQ(QRect(10, 20, 64, 30), children_[0]->geometry());
  EXPECT_EQ(QRect(78, 20, 64, 30), children_[1]->geometry());
  EXPECT_EQ(QRect(146, 20, 64, 30), children_[2]->geometry());
  EXPECT_EQ(QRect(10, 54, 64, 30), children_[3]->geometry());
  EXPECT_EQ(QRect(146, 54, 64, 30), children_[5]->geometry());

}

TEST_F(GridFlowLayoutTest, RespectsContentsMargins) {

  GridFlowLayout *layout = CreateLayout(0, 0);
  layout->setContentsMargins(5, 6, 7, 8);
  EXPECT_EQ(QSize(72, 44), layout->minimumSize());
  EXPECT_EQ(2, layout->ColumnsForWidth(132));
  EXPECT_EQ(6 + 3 * 30 + 8, layout->heightForWidth(132));
  layout->setGeometry(QRect(0, 0, 132, 104));
  EXPECT_EQ(QRect(5, 6, 60, 30), children_[0]->geometry());
  EXPECT_EQ(QRect(65, 66, 60, 30), children_[5]->geometry());

}

TEST_F(GridFlowLayoutTest, SkipsHiddenItems) {

  GridFlowLayout *layout = CreateLayout(0, 0);
  children_[1]->hide();
  children_[2]->hide();
  EXPECT_EQ(QSize(50, 20), layout->minimumSize());
  EXPECT_EQ(4, layout->ColumnsForWidth(1000));
  layout->setGeometry(QRect(0, 0, 200, 20));
  EXPECT_EQ(QRect(50, 0, 50, 20), children_[3]->geometry());

}

TEST_F(GridFlowLayoutTest, TakesAndDeletesItems) {

  GridFlowLayout *layout = CreateLayout(0, 0);
  EXPECT_EQ(6, layout->count());
  EXPECT_EQ(nullptr, layout->takeAt(-1));
  EXPECT_EQ(nullptr, layout->takeAt(6));
  QLayoutItem *item = layout->takeAt(0);
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(children_[0], item->widget());
  delete item;
  EXPECT_EQ(5, layout->count());
  EXPECT_EQ(children_[1], layout->itemAt(0)->widget());

}

TEST(GridFlowLayoutToolButtonTest, EqualCellsAlignButtonsWithDifferentLabels) {
  QWidget container;
  auto *layout = new GridFlowLayout(0, 3);
  container.setLayout(layout);
  QList<QToolButton*> buttons;
  for (const QString &text : {QStringLiteral("New"), QStringLiteral("Edit"), QStringLiteral("Delete"),
                              QStringLiteral("Restore defaults"), QStringLiteral("Open")}) {
    auto *button = new QToolButton(&container);
    button->setText(text);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setAutoRaise(true);
    button->setMinimumSize(80, 64);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(button);
    buttons.append(button);
  }
  buttons[1]->setEnabled(false);
  buttons[2]->setEnabled(false);
  container.resize(360, 160);
  container.show();
  QCoreApplication::processEvents();
  for (const Qt::ToolButtonStyle style : {Qt::ToolButtonIconOnly, Qt::ToolButtonTextUnderIcon}) {
    for (QToolButton *button : std::as_const(buttons)) button->setToolButtonStyle(style);
    for (const int width : {360, 240}) {
      layout->setGeometry(QRect(0, 0, width, layout->heightForWidth(width)));
      const int columns = layout->ColumnsForWidth(width);
      for (int i = 0; i < buttons.size(); ++i) {
        EXPECT_EQ(buttons[0]->geometry().size(), buttons[i]->geometry().size());
        EXPECT_GE(buttons[i]->width(), 80);
        EXPECT_GE(buttons[i]->height(), 64);
        if (i >= columns) {
          EXPECT_EQ(buttons[i % columns]->x(), buttons[i]->x());
        }
      }
    }
  }
}
