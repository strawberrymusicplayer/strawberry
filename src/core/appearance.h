/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QList>
#include <QMap>
#include <QString>
#include <QColor>
#include <QPalette>

class Appearance : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Appearance)

 public:
  explicit Appearance(QObject *parent = nullptr);

  // A configurable QPalette color role and the settings key its custom color is stored under.
  struct ColorRole {
    QPalette::ColorRole role;
    QString settings_key;
  };

  // The "central roles" from https://doc.qt.io/qt-6/qpalette.html, in the order shown in the settings page.
  static const QList<ColorRole> &ColorRoles();

  // A suitable set of colors for a dark theme, keyed by palette role (covers all of ColorRoles()).
  static const QMap<QPalette::ColorRole, QColor> &DarkColors();
  
  // The default style, captured before any user theme is applied so it can be restored later.
  const QString &default_style() const { return default_style_; }
  void set_default_style(const QString &style) { default_style_ = style; }

  // The system's default palette, captured before any user theme is applied so it can be restored later.
  const QPalette &system_palette() const { return system_palette_; }
  void set_system_palette(const QPalette &palette) { system_palette_ = palette; }

  void LoadCustomPaletteColors();

  // Applies the given per-role colors on top of the current application palette.
  static void SetCustomPaletteColors(const QMap<QPalette::ColorRole, QColor> &colors);

 private:
  QString default_style_;
  QPalette system_palette_;
};

#endif  // APPEARANCE_H
