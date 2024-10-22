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

#ifndef SETTINGSITEMDELEGATE_H
#define SETTINGSITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>

class QPainter;

class SettingsItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit SettingsItemDelegate(QObject *parent);
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
};

#endif  // SETTINGSITEMDELEGATE_H
