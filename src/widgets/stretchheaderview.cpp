/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <numeric>

#include <QtGlobal>
#include <QWidget>
#include <QHeaderView>
#include <QAbstractItemModel>
#include <QIODevice>
#include <QList>
#include <QByteArray>
#include <QDataStream>
#include <QResizeEvent>
#include <QMouseEvent>

#include "stretchheaderview.h"

namespace {
constexpr int kMagicNumber = 0x502C9510;
}

StretchHeaderView::StretchHeaderView(const Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent),
      stretch_enabled_(false),
      in_mouse_move_event_(false),
      forced_resize_logical_index_(-1) {

  setDefaultSectionSize(100);
  setMinimumSectionSize(30);
  setTextElideMode(Qt::ElideRight);

  QObject::connect(this, &StretchHeaderView::sectionResized, this, &StretchHeaderView::SectionResized);

}

void StretchHeaderView::setModel(QAbstractItemModel *model) {

  QHeaderView::setModel(model);

  if (stretch_enabled_) {
    column_widths_.resize(count());
    std::fill(column_widths_.begin(), column_widths_.end(), 1.0 / count());
  }

}

void StretchHeaderView::resizeEvent(QResizeEvent *e) {

  QHeaderView::resizeEvent(e);

  if (stretch_enabled_) {
    ResizeSections();
  }

}

void StretchHeaderView::mouseMoveEvent(QMouseEvent *e) {

  in_mouse_move_event_ = true;
  QHeaderView::mouseMoveEvent(e);
  in_mouse_move_event_ = false;

}

QByteArray StretchHeaderView::SaveState() const {

  QList<int> visual_indices;
  QList<int> column_pixel_widths;
  QList<int> columns_visible;
  visual_indices.reserve(count());
  column_pixel_widths.reserve(count());
  for (int i = 0; i < count(); ++i) {
    visual_indices << logicalIndex(i);
    column_pixel_widths << sectionSize(i);
    if (!isSectionHidden(i)) {
      columns_visible << i;
    }
  }

  QByteArray state;
  QDataStream s(&state, QIODevice::WriteOnly);
  s.setVersion(QDataStream::Qt_5_12);
  s << kMagicNumber;
  s << stretch_enabled_;
  s << visual_indices;
  s << column_pixel_widths;
  s << columns_visible;
  s << column_widths_;
  s << static_cast<int>(sortIndicatorOrder());
  s << sortIndicatorSection();

  return state;

}

bool StretchHeaderView::RestoreState(const QByteArray &state) {

  if (state.isEmpty()) return false;

  int magic_number = 0;
  QList<int> visual_indices;
  QList<int> column_pixel_widths;
  QList<int> columns_visible;
  int sort_indicator_order = Qt::AscendingOrder;
  int sort_indicator_section = 0;

  QDataStream s(state);
  s.setVersion(QDataStream::Qt_5_12);

  s >> magic_number;

  if (magic_number != kMagicNumber || s.atEnd()) {
    return false;
  }

  s >> stretch_enabled_;
  s >> visual_indices;
  s >> column_pixel_widths;
  s >> columns_visible;
  s >> column_widths_;
  s >> sort_indicator_order;
  s >> sort_indicator_section;

  for (int i = 0; i < count(); ++i) {
    if (i < visual_indices.count()) {
      moveSection(visualIndex(visual_indices[i]), i);
    }
    if (i < column_pixel_widths.count() && column_pixel_widths[i] > 0) {
      resizeSection(i, column_pixel_widths[i]);
    }
    setSectionHidden(i, !columns_visible.contains(i));
  }

  // Have we added more columns since the last time?
  while (column_widths_.count() < count()) {
    column_widths_ << 0;
  }

  setSortIndicator(sort_indicator_section, static_cast<Qt::SortOrder>(sort_indicator_order));

  if (stretch_enabled_) {
    // In stretch mode, we've already set the proportional column widths so apply them now.
    ResizeSections();
  }

  Q_EMIT StretchEnabledChanged(stretch_enabled_);

  return true;

}

QByteArray StretchHeaderView::ResetState() {

  stretch_enabled_ = false;
  column_widths_.resize(count());
  std::fill(column_widths_.begin(), column_widths_.end(), 1.0 / count());

  setSortIndicator(-1, Qt::AscendingOrder);

  for (int i = 0; i < count(); ++i) {
    setSectionHidden(i, false);
    resizeSection(i, defaultSectionSize());
    moveSection(visualIndex(i), i);
  }

  return SaveState();

}

void StretchHeaderView::ToggleStretchEnabled() {
  SetStretchEnabled(!is_stretch_enabled());
}

void StretchHeaderView::SetStretchEnabled(const bool enabled) {

  stretch_enabled_ = enabled;

  if (enabled) {
    // Initialize the list of widths from the current state of the widget
    column_widths_.resize(count());
    for (int i = 0; i < count(); ++i) {
      column_widths_[i] = static_cast<ColumnWidthType>(sectionSize(i)) / width();
    }

    // Stretch the columns to fill the widget
    NormaliseWidths();
    ResizeSections();
  }

  Q_EMIT StretchEnabledChanged(enabled);

}

void StretchHeaderView::SetColumnWidth(const int logical_index, const ColumnWidthType width) {

  column_widths_[logical_index] = width;

  if (!stretch_enabled_) return;

  QList<int> other_columns;
  for (int i = 0; i < count(); ++i) {
    if (!isSectionHidden(i) && i != logical_index) {
      other_columns << i;
    }
  }

  NormaliseWidths(other_columns);

}

void StretchHeaderView::NormaliseWidths(const QList<int> &sections) {

  if (!stretch_enabled_) return;

  const ColumnWidthType total_sum = std::accumulate(column_widths_.begin(), column_widths_.end(), 0.0);
  ColumnWidthType selected_sum = total_sum;

  if (!sections.isEmpty()) {
    selected_sum = 0.0;
    for (int i = 0; i < count(); ++i) {
      if (sections.contains(i)) {
        selected_sum += column_widths_.value(i);
      }
    }
  }

  if (total_sum != 0.0 && !qFuzzyCompare(total_sum, 1.0)) {
    const ColumnWidthType mult = (selected_sum + (1.0 - total_sum)) / selected_sum;
    for (int i = 0; i < column_widths_.count(); ++i) {
      if (sections.isEmpty() || sections.contains(i)) {
        column_widths_[i] *= mult;
      }
    }
  }

}

void StretchHeaderView::ResizeSections(const QList<int> &sections) {

  if (!stretch_enabled_) return;

  for (int i = 0; i < column_widths_.count(); ++i) {
    if (isSectionHidden(i) || (!sections.isEmpty() && !sections.contains(i))) {
      continue;
    }
    const int pixels = static_cast<int>(column_widths_.value(i) * width());
    if (pixels != 0) {
      resizeSection(i, pixels);
    }
  }

}

void StretchHeaderView::ShowSection(const int logical_index) {

  showSection(logical_index);

  if (stretch_enabled_) {

    // How many sections are visible already?
    int visible_count = 0;
    for (int i = 0; i < count(); ++i) {
      if (!isSectionHidden(i)) {
        ++visible_count;
      }
    }

    column_widths_[logical_index] = visible_count == 0 ? 1.0 : 1.0 / visible_count;

    NormaliseWidths();
    ResizeSections();

  }

  else {
    if (sectionSize(logical_index) == 0) {
      resizeSection(logical_index, defaultSectionSize());
    }
  }

}

void StretchHeaderView::HideSection(const int logical_index) {

  // Would this hide the last section?
  bool all_hidden = true;
  for (int i = 0; i < count(); ++i) {
    if (i != logical_index && !isSectionHidden(i) && sectionSize(i) > 0) {
      all_hidden = false;
      break;
    }
  }
  if (all_hidden) {
    return;
  }

  hideSection(logical_index);

  if (stretch_enabled_) {
    column_widths_[logical_index] = 0.0;
    NormaliseWidths();
    ResizeSections();
  }

}

void StretchHeaderView::SetSectionHidden(const int logical_index, const bool hidden) {

  if (hidden) {
    HideSection(logical_index);
  }
  else {
    ShowSection(logical_index);
  }

}

void StretchHeaderView::SectionResized(const int logical_index, const int old_size, const int new_size) {

  if (!stretch_enabled_) {
    return;
  }

  if (logical_index == forced_resize_logical_index_) {
    forced_resize_logical_index_ = -1;
    return;
  }

  if (!in_mouse_move_event_) {
    return;
  }

  bool resized = false;
  if (new_size >= minimumSectionSize()) {
    // Find the visible section to the right of the section that's being resized
    const int visual_index = visualIndex(logical_index);
    int right_section_logical_index = -1;
    int right_section_visual_index = -1;
    for (int i = 0; i <= count(); ++i) {
      if (!isSectionHidden(i) &&
          visualIndex(i) > visual_index &&
          (right_section_visual_index == -1 || visualIndex(i) < right_section_visual_index)) {
        right_section_logical_index = i;
        right_section_visual_index = visualIndex(i);
      }
    }
    if (right_section_logical_index != -1) {
      const int right_section_size = sectionSize(right_section_logical_index) + (old_size - new_size);
      if (right_section_size >= minimumSectionSize()) {
        column_widths_[logical_index] = static_cast<ColumnWidthType>(new_size) / width();
        column_widths_[right_section_logical_index] = static_cast<ColumnWidthType>(right_section_size) / width();
        in_mouse_move_event_ = false;
        NormaliseWidths(QList<int>() << right_section_logical_index);
        ResizeSections(QList<int>() << right_section_logical_index);
        in_mouse_move_event_ = true;
        resized = true;
      }
    }
  }

  if (!resized) {
    forced_resize_logical_index_ = logical_index;
    in_mouse_move_event_ = false;
    resizeSection(logical_index, old_size);
    in_mouse_move_event_ = true;
  }

}
