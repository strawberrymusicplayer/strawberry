/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QMap>
#include <QPalette>
#include <QString>

class QWidget;
class QTimer;
class QEvent;

class StyleSheetLoader : public QObject {
 Q_OBJECT

 public:
  explicit StyleSheetLoader(QObject *parent = nullptr);

  // Sets the given stylesheet on the given widget.
  // If the stylesheet contains strings like %palette-[role], these get replaced with actual palette colours.
  // The stylesheet is reloaded when the widget's palette changes.
  void SetStyleSheet(QWidget *widget, const QString& filename);

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  struct StyleSheetData {
    StyleSheetData() : count_(0) {}
    QString filename_;
    QString stylesheet_template_;
    QString stylesheet_current_;
    int count_;
  };

 private:
  void UpdateStyleSheet(QWidget *widget, StyleSheetData styledata);
  void ReplaceColor(QString *css, const QString name, const QPalette &palette, QPalette::ColorRole role) const;

 private slots:
  void ResetCounters();

 private:
  QMap<QWidget*, StyleSheetData> styledata_;
  QTimer *timer_reset_counter_;
};

#endif  // STYLESHEETLOADER_H
