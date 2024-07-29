/*
 * Strawberry Music Player
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

#include <QObject>
#include <QString>
#include <QIcon>
#include <QVBoxLayout>

#include "fancytabdata.h"

FancyTabData::FancyTabData(QWidget *widget_view, const QString &name, const QIcon &icon, const QString &label, const int idx, QWidget *parent)
    : QObject(parent),
      widget_view_(widget_view),
      name_(name), icon_(icon),
      label_(label),
      index_(idx),
      page_(new QWidget()) {

  // In order to achieve the same effect as the "Bottom Widget" of the old Nokia based FancyTabWidget a VBoxLayout is used on each page

  QVBoxLayout *layout = new QVBoxLayout(page_);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(widget_view_);
  page_->setLayout(layout);

}
