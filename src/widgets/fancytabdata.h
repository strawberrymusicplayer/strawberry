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

#ifndef FANCYTABDATA_H
#define FANCYTABDATA_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QIcon>

class FancyTabData : public QObject {
  Q_OBJECT

 public:
  explicit FancyTabData(QWidget *widget_view, const QString &name, const QIcon &icon, const QString &label, const int idx, QWidget *parent);

  QWidget *widget_view() const { return widget_view_; }
  QString name() const { return name_; }
  QIcon icon() const { return icon_; }
  QString label() const { return label_; }
  QWidget *page() const { return page_; }
  int index() const { return index_; }

 private:
  QWidget *widget_view_;
  QString name_;
  QIcon icon_;
  QString label_;
  int index_;
  QWidget *page_;
};

#endif  // FANCYTABDATA_H
