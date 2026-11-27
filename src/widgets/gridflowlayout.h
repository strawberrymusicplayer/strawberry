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

#ifndef GRIDFLOWLAYOUT_H
#define GRIDFLOWLAYOUT_H

#include <QLayout>
#include <QList>
#include <QRect>
#include <QSize>

class QLayoutItem;

// Arranges items in a grid of equally sized cells that wraps to as many columns as fit the available width.
// The cells are stretched to fill each row, and the height follows from the width.
class GridFlowLayout : public QLayout {
 public:
  explicit GridFlowLayout(const int spacing = 0, const int maximum_columns = 0);
  ~GridFlowLayout() override;

  // 0 allows as many columns as there are visible items.
  int maximum_columns() const { return maximum_columns_; }
  int ColumnsForWidth(const int width) const;

  void addItem(QLayoutItem *item) override;
  int count() const override;
  QLayoutItem *itemAt(const int index) const override;
  QLayoutItem *takeAt(const int index) override;
  Qt::Orientations expandingDirections() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(const int width) const override;
  QSize minimumSize() const override;
  QSize sizeHint() const override;
  void setGeometry(const QRect &rect) override;

 private:
  QList<QLayoutItem*> VisibleItems() const;
  QSize CellSize() const;
  int Spacing() const;
  int Arrange(const QRect &rect, const bool apply) const;

  int maximum_columns_;
  QList<QLayoutItem*> items_;
};

#endif  // GRIDFLOWLAYOUT_H
