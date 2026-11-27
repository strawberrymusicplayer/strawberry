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

#include <algorithm>

#include <QLayoutItem>
#include <QMargins>

#include "gridflowlayout.h"

GridFlowLayout::GridFlowLayout(const int spacing, const int maximum_columns)
    : maximum_columns_(maximum_columns) {

  setContentsMargins(0, 0, 0, 0);
  setSpacing(spacing);

}

GridFlowLayout::~GridFlowLayout() {
  while (QLayoutItem *item = takeAt(0)) delete item;
}

void GridFlowLayout::addItem(QLayoutItem *item) {
  items_.append(item);
  invalidate();
}

int GridFlowLayout::count() const { return static_cast<int>(items_.count()); }

QLayoutItem *GridFlowLayout::itemAt(const int index) const { return items_.value(index); }

QLayoutItem *GridFlowLayout::takeAt(const int index) {

  if (index < 0 || index >= count()) return nullptr;
  QLayoutItem *item = items_.takeAt(index);
  invalidate();
  return item;

}

Qt::Orientations GridFlowLayout::expandingDirections() const { return Qt::Horizontal; }

bool GridFlowLayout::hasHeightForWidth() const { return true; }

int GridFlowLayout::heightForWidth(const int width) const {
  return Arrange(QRect(0, 0, width, 0), false);
}

QSize GridFlowLayout::minimumSize() const {

  const QMargins margins = contentsMargins();
  return CellSize() + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());

}

QSize GridFlowLayout::sizeHint() const { return minimumSize(); }

void GridFlowLayout::setGeometry(const QRect &rect) {

  QLayout::setGeometry(rect);
  Arrange(rect, true);

}

QList<QLayoutItem*> GridFlowLayout::VisibleItems() const {

  QList<QLayoutItem*> visible_items;
  for (QLayoutItem *item : items_) {
    if (!item->isEmpty()) visible_items.append(item);
  }
  return visible_items;

}

QSize GridFlowLayout::CellSize() const {

  QSize size(0, 0);
  for (QLayoutItem *item : VisibleItems()) size = size.expandedTo(item->minimumSize());
  return size;

}

int GridFlowLayout::Spacing() const { return std::max(0, spacing()); }

int GridFlowLayout::ColumnsForWidth(const int width) const {

  const int item_count = static_cast<int>(VisibleItems().count());
  if (item_count == 0) return 0;
  const QMargins margins = contentsMargins();
  const int available_width = width - margins.left() - margins.right();
  const int gap = Spacing();
  const int cell_width = std::max(1, CellSize().width());
  const int maximum_columns = maximum_columns_ > 0 ? std::min(maximum_columns_, item_count) : item_count;
  return std::clamp((available_width + gap) / (cell_width + gap), 1, maximum_columns);

}

int GridFlowLayout::Arrange(const QRect &rect, const bool apply) const {

  const QList<QLayoutItem*> visible_items = VisibleItems();
  if (visible_items.isEmpty()) return 0;

  const QRect contents = rect.marginsRemoved(contentsMargins());
  const int gap = Spacing();
  const int columns = ColumnsForWidth(rect.width());
  const int cell_width = std::max(0, (contents.width() - (columns - 1) * gap) / columns);
  const int cell_height = CellSize().height();
  if (apply) {
    for (int i = 0; i < visible_items.count(); ++i) {
      const int row = i / columns;
      const int column = i % columns;
      visible_items[i]->setGeometry(QRect(contents.x() + column * (cell_width + gap), contents.y() + row * (cell_height + gap), cell_width, cell_height));
    }
  }

  const int rows = (static_cast<int>(visible_items.count()) + columns - 1) / columns;
  const QMargins margins = contentsMargins();
  return margins.top() + rows * cell_height + (rows - 1) * gap + margins.bottom();

}
