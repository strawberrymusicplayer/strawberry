/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#include <stdbool.h>

#include <QObject>
#include <QWidget>
#include <QString>
#include <QColor>

#include "settingspage.h"

class SettingsDialog;
class Ui_AppearanceSettingsPage;

class AppearanceSettingsPage : public SettingsPage {
  Q_OBJECT

public:
  AppearanceSettingsPage(SettingsDialog *dialog);
  ~AppearanceSettingsPage();

  static const char *kSettingsGroup;

  static const char *kUseCustomColorSet;
  static const char *kForegroundColor;
  static const char *kBackgroundColor;

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

  enum BackgroundImageType {
    BackgroundImageType_Default,
    BackgroundImageType_None,
    BackgroundImageType_Custom,
    BackgroundImageType_Album
  };

  enum BackgroundImagePosition {
    BackgroundImagePosition_UpperLeft = 1,
    BackgroundImagePosition_UpperRight = 2,
    BackgroundImagePosition_Middle = 3,
    BackgroundImagePosition_BottomLeft = 4,
    BackgroundImagePosition_BottomRight = 5
  };

  void Load();
  void Save();
  void Cancel();

 private slots:
  void SelectForegroundColor();
  void SelectBackgroundColor();
  void UseCustomColorSetOptionChanged(bool);
  void SelectBackgroundImage();
  void BlurLevelChanged(int);
  void OpacityLevelChanged(int);

 private:

  // Set the widget's background to new_color
  void UpdateColorSelectorColor(QWidget *color_selector, const QColor &new_color);
  // Init (or refresh) the colorSelectors colors
  void InitColorSelectorsColors();

  Ui_AppearanceSettingsPage *ui_;

  bool original_use_a_custom_color_set_;
  QColor original_foreground_color_;
  QColor original_background_color_;
  QColor current_foreground_color_;
  QColor current_background_color_;
  BackgroundImageType background_image_type_;
  QString background_image_filename_;

};

#endif // APPEARANCESETTINGSPAGE_H
