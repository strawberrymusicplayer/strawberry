/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <utility>

#include <QWidget>
#include <QListView>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractItemDelegate>
#include <QItemSelectionModel>
#include <QList>
#include <QString>
#include <QFont>
#include <QFontMetrics>
#include <QLocale>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QPen>
#include <QPoint>
#include <QScrollBar>
#include <QSize>
#include <QStyle>
#include <QStyleOption>
#include <QFlags>
#include <QtEvents>

#include "core/multisortfilterproxy.h"
#include "groupediconview.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kBarThickness = 2;
constexpr int kBarMarginTop = 3;
}  // namespace

GroupedIconView::GroupedIconView(QWidget *parent)
    : QListView(parent),
      proxy_model_(new MultiSortFilterProxy(this)),
      default_header_height_(fontMetrics().height() +
      kBarMarginTop + kBarThickness),
      header_spacing_(10),
      header_indent_(5),
      item_indent_(10),
      header_text_(u"%1"_s) {

  setFlow(LeftToRight);
  setViewMode(IconMode);
  setResizeMode(Adjust);
  setWordWrap(true);
  setDragEnabled(false);

  proxy_model_->AddSortSpec(Role_Group);
  proxy_model_->setDynamicSortFilter(true);

  QObject::connect(proxy_model_, &MultiSortFilterProxy::modelReset, this, &GroupedIconView::LayoutItems);

}

void GroupedIconView::AddSortSpec(const int role, const Qt::SortOrder order) {
  proxy_model_->AddSortSpec(role, order);
}

void GroupedIconView::setModel(QAbstractItemModel *model) {

  proxy_model_->setSourceModel(model);
  proxy_model_->sort(0);

  QListView::setModel(proxy_model_);
  LayoutItems();

}

int GroupedIconView::header_height() const {
  return default_header_height_;
}

void GroupedIconView::DrawHeader(QPainter *painter, const QRect rect, const QFont &font, const QPalette &palette, const QString &text) {

  painter->save();

  // Bold font
  QFont bold_font(font);
  bold_font.setBold(true);
  QFontMetrics metrics(bold_font);

  QRect text_rect(rect);
  text_rect.setHeight(metrics.height());
  text_rect.moveTop(rect.top() + (rect.height() - text_rect.height() - kBarThickness - kBarMarginTop) / 2);
  text_rect.setLeft(text_rect.left() + 3);

  // Draw text
  painter->setFont(bold_font);
  painter->drawText(text_rect, text);

  // Draw a line underneath
  const QPoint start(rect.left(), text_rect.bottom() + kBarMarginTop);
  const QPoint end(rect.right(), start.y());

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(QPen(palette.color(QPalette::Disabled, QPalette::Text), kBarThickness, Qt::SolidLine, Qt::RoundCap));
  painter->setOpacity(0.5);
  painter->drawLine(start, end);

  painter->restore();
}

void GroupedIconView::resizeEvent(QResizeEvent *e) {
  QListView::resizeEvent(e);
  LayoutItems();
}

void GroupedIconView::rowsInserted(const QModelIndex &parent, int start, int end) {
  QListView::rowsInserted(parent, start, end);
  LayoutItems();
}

void GroupedIconView::dataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right, const QList<int> &roles) {

  Q_UNUSED(roles)

  QListView::dataChanged(top_left, bottom_right);
  LayoutItems();

}

void GroupedIconView::LayoutItems() {

  if (!model()) {
    return;
  }

  const int count = model()->rowCount();

  QString last_group;
  QPoint next_position(0, 0);
  int max_row_height = 0;

  visual_rects_.clear();
  visual_rects_.reserve(count);
  headers_.clear();

  for (int i = 0; i < count; ++i) {
    const QModelIndex idx(model()->index(i, 0));
    const QString group = idx.data(Role_Group).toString();
    const QSize size(rectForIndex(idx).size());

    // Is this the first item in a new group?
    if (group != last_group) {
      // Add the group header.
      Header header;
      header.y = next_position.y() + max_row_height + header_indent_;
      header.first_row = i;
      header.text = group;

      if (!last_group.isNull()) {
        header.y += header_spacing_;
      }

      headers_ << header;

      // Remember this group, so we don't add it again.
      last_group = group;

      // Move the next item immediately below the header.
      next_position.setX(0);
      next_position.setY(header.y + header_height() + header_indent_ + header_spacing_);
      max_row_height = 0;
    }

    // Take into account padding and spacing
    QPoint this_position(next_position);
    if (this_position.x() == 0) {
      this_position.setX(this_position.x() + item_indent_);
    }
    else {
      this_position.setX(this_position.x() + spacing());
    }

    // Should this item wrap?
    if (next_position.x() != 0 && this_position.x() + size.width() >= viewport()->width()) {
      next_position.setX(0);
      next_position.setY(next_position.y() + max_row_height);
      this_position = next_position;
      this_position.setX(this_position.x() + item_indent_);

      max_row_height = 0;
    }

    // Set this item's geometry
    visual_rects_.append(QRect(this_position, size));

    // Update next index
    next_position.setX(this_position.x() + size.width());
    max_row_height = qMax(max_row_height, size.height());
  }

  verticalScrollBar()->setRange(0, next_position.y() + max_row_height - viewport()->height());
  update();

}

QRect GroupedIconView::visualRect(const QModelIndex &idx) const {

  if (idx.row() < 0 || idx.row() >= visual_rects_.count()) {
    return QRect();
  }
  return visual_rects_[idx.row()].translated(-horizontalOffset(), -verticalOffset());

}

QModelIndex GroupedIconView::indexAt(const QPoint &p) const {

  const QPoint viewport_p = p + QPoint(horizontalOffset(), verticalOffset());

  const int count = static_cast<int>(visual_rects_.count());
  for (int i = 0; i<count; ++i) {
    if (visual_rects_[i].contains(viewport_p)) {
      return model()->index(i, 0);
    }
  }
  return QModelIndex();

}

void GroupedIconView::paintEvent(QPaintEvent *e) {

  // This code was adapted from QListView::paintEvent(), changed to use the visualRect() of items, and to draw headers.

  QStyleOptionViewItem option;
  initViewItemOption(&option);
  if (isWrapping()) {
    option.features = QStyleOptionViewItem::WrapText;
  }
  option.locale = locale();
  option.locale.setNumberOptions(QLocale::OmitGroupSeparator);
  option.widget = this;

  QPainter painter(viewport());

  const QRect viewport_rect(e->rect().translated(horizontalOffset(), verticalOffset()));
  QList<QModelIndex> toBeRendered = IntersectingItems(viewport_rect);

  const QModelIndex current = currentIndex();
  const QAbstractItemModel *itemModel = model();
  const QItemSelectionModel *selections = selectionModel();
  const bool focus = (hasFocus() || viewport()->hasFocus()) && current.isValid();
  const QStyle::State opt_state = option.state;
  const QAbstractItemView::State viewState = state();
  const bool enabled = (opt_state & QStyle::State_Enabled) != 0;

  int maxSize = (flow() == TopToBottom) ? viewport()->size().width() - 2 * spacing() : viewport()->size().height() - 2 * spacing();

  QList<QModelIndex>::const_iterator end = toBeRendered.constEnd();
  for (QList<QModelIndex>::const_iterator it = toBeRendered.constBegin(); it != end; ++it) {
    if (!it->isValid()) {
      continue;
    }

    option.rect = visualRect(*it);

    if (flow() == TopToBottom) {
      option.rect.setWidth(qMin(maxSize, option.rect.width()));
    }
    else {
      option.rect.setHeight(qMin(maxSize, option.rect.height()));
    }

    option.state = opt_state;
    if (selections && selections->isSelected(*it))
      option.state |= QStyle::State_Selected;
    if (enabled) {
      QPalette::ColorGroup cg = QPalette::Active;
      if ((itemModel->flags(*it) & Qt::ItemIsEnabled) == 0) {
        option.state &= ~QStyle::State_Enabled;
        cg = QPalette::Disabled;
      }
      else {
        cg = QPalette::Normal;
      }
      option.palette.setCurrentColorGroup(cg);
    }
    if (focus && current == *it) {
      option.state |= QStyle::State_HasFocus;
      if (viewState == EditingState) {
        option.state |= QStyle::State_Editing;
      }
    }

    itemDelegate()->paint(&painter, option, *it);
  }

  // Draw headers
  for (const Header &header : std::as_const(headers_)) {
    const QRect header_rect = QRect(header_indent_, header.y, viewport()->width() - header_indent_ * 2, header_height());

    // Is this header contained in the area we're drawing?
    if (!header_rect.intersects(viewport_rect)) {
      continue;
    }

    // Draw the header
    DrawHeader(&painter,
               header_rect.translated(-horizontalOffset(), -verticalOffset()),
               font(),
               palette(),
               model()->index(header.first_row, 0).data(Role_Group).toString());
  }

}

void GroupedIconView::setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags command) {

  const QList<QModelIndex> indexes(IntersectingItems(rect.translated(horizontalOffset(), verticalOffset())));

  QItemSelection selection;
  selection.reserve(indexes.count());

  for (const QModelIndex &idx : indexes) {
    selection << QItemSelectionRange(idx);
  }

  selectionModel()->select(selection, command);

}

QList<QModelIndex> GroupedIconView::IntersectingItems(const QRect rect) const {

  QList<QModelIndex> ret;

  const int count = static_cast<int>(visual_rects_.count());
  for (int i = 0; i < count; ++i) {
    if (rect.intersects(visual_rects_[i])) {
      ret.append(model()->index(i, 0));
    }
  }

  return ret;

}

QRegion GroupedIconView::visualRegionForSelection(const QItemSelection &selection) const {

  QRegion ret;
  const QModelIndexList indexes = selection.indexes();
  for (const QModelIndex &idx : indexes) {
    ret += visual_rects_[idx.row()];
  }
  return ret;

}

QModelIndex GroupedIconView::moveCursor(CursorAction action, const Qt::KeyboardModifiers keyboard_modifiers) {

  Q_UNUSED(keyboard_modifiers)

  if (model()->rowCount() == 0) {
    return QModelIndex();
  }

  int ret = currentIndex().row();
  if (ret == -1) {
    ret = 0;
  }

  switch (action) {
    case MoveUp:    ret = IndexAboveOrBelow(ret, -1); break;
    case MovePrevious:
    case MoveLeft:  ret --; break;
    case MoveDown:  ret = IndexAboveOrBelow(ret, +1); break;
    case MoveNext:
    case MoveRight: ret ++; break;
    case MovePageUp:
    case MoveHome:  ret = 0; break;
    case MovePageDown:
    case MoveEnd:   ret = model()->rowCount() - 1; break;
  }

  return model()->index(qBound(0, ret, model()->rowCount()), 0);

}

int GroupedIconView::IndexAboveOrBelow(int index, const int d) const {

  const QRect orig_rect(visual_rects_[index]);

  while (index >= 0 && index < visual_rects_.count()) {
    const QRect rect(visual_rects_[index]);
    const QPoint center(rect.center());

    if ((center.y() <= orig_rect.top() || center.y() >= orig_rect.bottom()) && center.x() >= orig_rect.left() && center.x() <= orig_rect.right()) {
      return index;
    }

    index += d;
  }

  return index;

}
