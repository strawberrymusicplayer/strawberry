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
#include <QThread>
#include <QList>
#include <QMap>
#include <QPalette>
#include <QColor>

#include "settings.h"
#include "appearance.h"
#include "constants/appearancesettings.h"

Appearance::Appearance(QObject *parent) : QObject(parent), system_palette_(QApplication::palette()) {}

namespace {

bool IsTextColorRole(const QPalette::ColorRole color_role) {

  return color_role == QPalette::WindowText || color_role == QPalette::Text || color_role == QPalette::ButtonText || color_role == QPalette::BrightText || color_role == QPalette::PlaceholderText;

}

QPalette::ColorRole BackgroundRoleForTextRole(const QPalette::ColorRole color_role) {

  switch (color_role) {
    case QPalette::Text:
    case QPalette::PlaceholderText:
      return QPalette::Base;
    case QPalette::ButtonText:
      return QPalette::Button;
    default:
      return QPalette::Window;
  }

}

QColor DimmedTextColor(const QColor &text_color, const QColor &background_color) {

  return QColor((text_color.red() + background_color.red()) / 2, (text_color.green() + background_color.green()) / 2, (text_color.blue() + background_color.blue()) / 2);

}

}  // namespace

const QList<Appearance::ColorRole> &Appearance::ColorRoles() {

  static const QList<ColorRole> color_roles = {
    { QPalette::Window, QLatin1String(AppearanceSettings::kColorWindow) },
    { QPalette::WindowText, QLatin1String(AppearanceSettings::kColorWindowText) },
    { QPalette::Base, QLatin1String(AppearanceSettings::kColorBase) },
    { QPalette::AlternateBase, QLatin1String(AppearanceSettings::kColorAlternateBase) },
    { QPalette::Text, QLatin1String(AppearanceSettings::kColorText) },
    { QPalette::Button, QLatin1String(AppearanceSettings::kColorButton) },
    { QPalette::ButtonText, QLatin1String(AppearanceSettings::kColorButtonText) },
    { QPalette::BrightText, QLatin1String(AppearanceSettings::kColorBrightText) },
    { QPalette::PlaceholderText, QLatin1String(AppearanceSettings::kColorPlaceholderText) },
    { QPalette::ToolTipBase, QLatin1String(AppearanceSettings::kColorToolTipBase) },
    { QPalette::ToolTipText, QLatin1String(AppearanceSettings::kColorToolTipText) }
  };

  return color_roles;

}

const QMap<QPalette::ColorRole, QColor> &Appearance::DarkColors() {

  static const QMap<QPalette::ColorRole, QColor> dark_colors = {
    { QPalette::Window, QColor(53, 53, 53) },
    { QPalette::WindowText, QColor(240, 240, 240) },
    { QPalette::Base, QColor(35, 35, 35) },
    { QPalette::AlternateBase, QColor(53, 53, 53) },
    { QPalette::Text, QColor(240, 240, 240) },
    { QPalette::Button, QColor(53, 53, 53) },
    { QPalette::ButtonText, QColor(240, 240, 240) },
    { QPalette::BrightText, QColor(255, 80, 80) },
    { QPalette::PlaceholderText, QColor(140, 140, 140) },
    { QPalette::ToolTipBase, QColor(53, 53, 53) },
    { QPalette::ToolTipText, QColor(240, 240, 240) }
  };

  return dark_colors;

}

void Appearance::LoadCustomPaletteColors() {

  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  const bool use_custom_color_set = s.value(AppearanceSettings::kUseCustomColorSet).toBool();

  if (use_custom_color_set) {
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
    SetCustomPaletteColors(colors);
  }

  s.endGroup();

}

void Appearance::SetCustomPaletteColors(const QMap<QPalette::ColorRole, QColor> &colors) {

  Q_ASSERT(QThread::currentThread() == qApp->thread());

  // Only set the active and inactive color groups here, the disabled color group is handled below so that disabled widgets still look greyed out with a custom color set.
  QPalette palette = QApplication::palette();
  for (QMap<QPalette::ColorRole, QColor>::const_iterator it = colors.constBegin(); it != colors.constEnd(); ++it) {
    if (it.value().isValid()) {
      palette.setColor(QPalette::Active, it.key(), it.value());
      palette.setColor(QPalette::Inactive, it.key(), it.value());
      if (!IsTextColorRole(it.key())) {
        palette.setColor(QPalette::Disabled, it.key(), it.value());
      }
    }
  }

  // Dim the text roles for the disabled color group by blending them with the corresponding background color.
  for (QMap<QPalette::ColorRole, QColor>::const_iterator it = colors.constBegin(); it != colors.constEnd(); ++it) {
    if (it.value().isValid() && IsTextColorRole(it.key())) {
      const QColor background_color = palette.color(QPalette::Disabled, BackgroundRoleForTextRole(it.key()));
      palette.setColor(QPalette::Disabled, it.key(), DimmedTextColor(it.value(), background_color));
    }
  }

  QApplication::setPalette(palette);

}
