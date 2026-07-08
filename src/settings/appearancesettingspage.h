/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef APPEARANCESETTINGSPAGE_H
#define APPEARANCESETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QColor>
#include <QPalette>

#include "includes/shared_ptr.h"
#include "settingspage.h"
#include "constants/appearancesettings.h"

class QWidget;
class QPushButton;

class SettingsDialog;
class Ui_AppearanceSettingsPage;
class Appearance;

class AppearanceSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit AppearanceSettingsPage(SettingsDialog *dialog, SharedPtr<Appearance> appearance, QWidget *parent = nullptr);
  ~AppearanceSettingsPage() override;

  void Load() override;
  void Save() override;

 private Q_SLOTS:
  void StyleChanged(const int index);
  void DarkModeToggled(const bool checked);
  void UseCustomColorSetOptionChanged(const bool checked);
  void SetDarkColors();
  void ResetToDefaultColors();
  void SelectBackgroundImage();
  void BlurLevelChanged(const int value);
  void OpacityLevelChanged(const int percent);
  void TabBarSystemColor(const bool checked);
  void TabBarSelectBGColor();
  void PlaylistPlayingSongColorSystem(const bool checked);
  void PlaylistPlayingSongSelectColor();

 private:
  void Cancel() override;

  // Set the widget's background to new_color
  static void UpdateColorSelectorColor(QWidget *color_selector, const QColor &new_color);
  // Create a color selector button for each palette role in the custom colors layout.
  void CreateColorSelectors();
  // Refresh all custom color selector buttons from current_colors_.
  void UpdateColorSelectorsColors();
  // Open a color dialog for the given palette role and apply the result.
  void SelectColor(const QPalette::ColorRole role);
  // Apply current_colors_ to the application palette (live preview).
  void ApplyCustomColors();
  // Human-readable label for a palette color role.
  static QString ColorRoleLabel(const QPalette::ColorRole role);

  static bool IsNativeWindowsStyle(const QString &style_name);

  Ui_AppearanceSettingsPage *ui_;
  const SharedPtr<Appearance> appearance_;

  bool original_dark_mode_;
  bool original_use_custom_color_set_;
  QMap<QPalette::ColorRole, QColor> original_colors_;
  QMap<QPalette::ColorRole, QColor> current_colors_;
  QMap<QPalette::ColorRole, QPushButton*> color_selectors_;
  QColor current_tabbar_bg_color_;
  AppearanceSettings::BackgroundImageType background_image_type_;
  QString background_image_filename_;
  QColor current_playlist_playing_song_color_;
};

#endif  // APPEARANCESETTINGSPAGE_H
