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

#include "config.h"

#include <QApplication>
#include <QObject>
#include <QList>
#include <QMap>
#include <QPalette>
#include <QColor>

#include "settings.h"
#include "appearance.h"
#include "constants/appearancesettings.h"

Appearance::Appearance(QObject *parent) : QObject(parent), system_palette_(QApplication::palette()) {}

const QList<Appearance::ColorRole> &Appearance::ColorRoles() {

  static const QList<ColorRole> color_roles = {
    { QPalette::Window, QLatin1String(AppearanceSettings::kColorWindow) },
    { QPalette::WindowText, QLatin1String(AppearanceSettings::kColorWindowText) },
    { QPalette::Base, QLatin1String(AppearanceSettings::kColorBase) },
    { QPalette::AlternateBase, QLatin1String(AppearanceSettings::kColorAlternateBase) },
    { QPalette::ToolTipBase, QLatin1String(AppearanceSettings::kColorToolTipBase) },
    { QPalette::ToolTipText, QLatin1String(AppearanceSettings::kColorToolTipText) },
    { QPalette::Text, QLatin1String(AppearanceSettings::kColorText) },
    { QPalette::Button, QLatin1String(AppearanceSettings::kColorButton) },
    { QPalette::ButtonText, QLatin1String(AppearanceSettings::kColorButtonText) },
    { QPalette::BrightText, QLatin1String(AppearanceSettings::kColorBrightText) },
    { QPalette::PlaceholderText, QLatin1String(AppearanceSettings::kColorPlaceholderText) }
  };

  return color_roles;

}

QMap<QPalette::ColorRole, QColor> Appearance::DarkColors() {

  return QMap<QPalette::ColorRole, QColor>{
    { QPalette::Window, QColor(53, 53, 53) },
    { QPalette::WindowText, QColor(240, 240, 240) },
    { QPalette::Base, QColor(35, 35, 35) },
    { QPalette::AlternateBase, QColor(53, 53, 53) },
    { QPalette::ToolTipBase, QColor(53, 53, 53) },
    { QPalette::ToolTipText, QColor(240, 240, 240) },
    { QPalette::Text, QColor(240, 240, 240) },
    { QPalette::Button, QColor(53, 53, 53) },
    { QPalette::ButtonText, QColor(240, 240, 240) },
    { QPalette::BrightText, QColor(255, 80, 80) },
    { QPalette::PlaceholderText, QColor(140, 140, 140) }
  };

}

void Appearance::LoadUserTheme() {

  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  const bool use_custom_color_set = s.value(AppearanceSettings::kUseCustomColorSet).toBool();

  if (!use_custom_color_set) {
    s.endGroup();
    return;
  }

  QMap<QPalette::ColorRole, QColor> colors;
  for (const ColorRole &color_role : ColorRoles()) {
    const QVariant value = s.value(color_role.settings_key);
    if (value.isValid()) {
      const QColor color = value.value<QColor>();
      if (color.isValid()) {
        colors.insert(color_role.role, color);
      }
    }
  }

  s.endGroup();

  ChangeColors(colors);

}

void Appearance::ResetToSystemDefaultTheme() {
  QApplication::setPalette(system_palette_);
}

void Appearance::ChangeColors(const QMap<QPalette::ColorRole, QColor> &colors) {

  QPalette palette = QApplication::palette();

  for (QMap<QPalette::ColorRole, QColor>::const_iterator it = colors.constBegin(); it != colors.constEnd(); ++it) {
    if (it.value().isValid()) {
      palette.setColor(it.key(), it.value());
    }
  }

  QApplication::setPalette(palette);

}
