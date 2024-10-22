/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef STYLESHEETLOADER_H
#define STYLESHEETLOADER_H

#include "config.h"

#include <QObject>
#include <QPair>
#include <QHash>
#include <QPalette>
#include <QString>

#include "includes/shared_ptr.h"

class QWidget;
class QEvent;

class StyleSheetLoader : public QObject {
  Q_OBJECT

 public:
  explicit StyleSheetLoader(QObject *parent = nullptr);

  // Sets the given stylesheet on the given widget.
  // If the stylesheet contains strings like %palette-[role], these get replaced with actual palette colours.
  // The stylesheet is reloaded when the widget's palette changes.
  void SetStyleSheet(QWidget *widget, const QString &filename);

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  struct StyleSheetData {
    StyleSheetData() {}
    QString filename_;
    QString stylesheet_template_;
    QString stylesheet_current_;
  };

 private:
  void UpdateStyleSheet(QWidget *widget, SharedPtr<StyleSheetData> styledata);
  static void ReplaceColor(QString *css, const QString &name, const QPalette &palette, const QPalette::ColorRole role);

 private:
  QHash<QWidget*, SharedPtr<StyleSheetData>> styledata_;
};

#endif  // STYLESHEETLOADER_H
