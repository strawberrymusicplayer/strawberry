/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "settingsitemdelegate.h"
#include "widgets/groupediconview.h"
#include "settingsdialog.h"

using namespace Qt::Literals::StringLiterals;

SettingsItemDelegate::SettingsItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QSize SettingsItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const {

  const bool is_separator = idx.data(SettingsDialog::Role_IsSeparator).toBool();
  QSize ret = QStyledItemDelegate::sizeHint(option, idx);

  if (is_separator) {
    ret.setHeight(ret.height() * 2);
  }

  return ret;

}

void SettingsItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const {

  const bool is_separator = idx.data(SettingsDialog::Role_IsSeparator).toBool();

  if (is_separator) {
    GroupedIconView::DrawHeader(painter, option.rect, option.font, option.palette, idx.data().toString());
  }
  else {
    QStyledItemDelegate::paint(painter, option, idx);
  }

}
