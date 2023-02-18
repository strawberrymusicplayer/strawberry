/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#ifndef APPEARANCESETTINGSPAGE_H
#define APPEARANCESETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QColor>

#include "settingspage.h"

class QWidget;

class SettingsDialog;
class Ui_AppearanceSettingsPage;

class AppearanceSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit AppearanceSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~AppearanceSettingsPage() override;

  static const char *kSettingsGroup;

  static const char *kStyle;

  static const char *kBackgroundImageType;
  static const char *kBackgroundImageFilename;
  static const char *kBackgroundImagePosition;
  static const char *kBackgroundImageStretch;
  static const char *kBackgroundImageDoNotCut;
  static const char *kBackgroundImageKeepAspectRatio;
  static const char *kBackgroundImageMaxSize;

  static const char *kBlurRadius;
  static const char *kOpacityLevel;

  static const int kDefaultBlurRadius;
  static const int kDefaultOpacityLevel;

  static const char *kSystemThemeIcons;

  static const char *kTabBarSystemColor;
  static const char *kTabBarGradient;
  static const char *kTabBarColor;

  static const char *kIconSizeTabbarSmallMode;
  static const char *kIconSizeTabbarLargeMode;
  static const char *kIconSizePlayControlButtons;
  static const char *kIconSizePlaylistButtons;
  static const char *kIconSizeLeftPanelButtons;
  static const char *kIconSizeConfigureButtons;

  static const char *kPlaylistPlayingSongColor;

  enum class BackgroundImageType {
    Default,
    None,
    Custom,
    Album,
    Strawbs
  };

  enum class BackgroundImagePosition {
    UpperLeft = 1,
    UpperRight = 2,
    Middle = 3,
    BottomLeft = 4,
    BottomRight = 5
  };

  void Load() override;
  void Save() override;

  static QColor DefaultTabbarBgColor();

 private slots:
  void SelectBackgroundImage();
  void BlurLevelChanged(int);
  void OpacityLevelChanged(int);
  void TabBarSystemColor(bool checked);
  void TabBarSelectBGColor();
  void PlaylistPlayingSongColorSystem(bool checked);
  void PlaylistPlayingSongSelectColor();

 private:
  // Set the widget's background to new_color
  static void UpdateColorSelectorColor(QWidget *color_selector, const QColor &new_color);

  Ui_AppearanceSettingsPage *ui_;

  QColor current_tabbar_bg_color_;
  BackgroundImageType background_image_type_;
  QString background_image_filename_;
  QColor current_playlist_playing_song_color_;

};

#endif  // APPEARANCESETTINGSPAGE_H
