/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, Arnaud Bienner <arnaud.bienner@gmail.com>
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

#ifndef APPEARANCE_H
#define APPEARANCE_H

#include "config.h"

#include <QObject>
#include <QColor>
#include <QPalette>

class Appearance : public QObject {
  Q_OBJECT

 public:
  explicit Appearance(QObject *parent = nullptr);

  static const QPalette kDefaultPalette;

  void LoadUserTheme();
  static void ResetToSystemDefaultTheme();
  void ChangeForegroundColor(const QColor &color);
  void ChangeBackgroundColor(const QColor &color);

 private:
  QColor foreground_color_;
  QColor background_color_;

};

#endif  // APPEARANCE_H
