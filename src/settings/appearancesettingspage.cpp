/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QWidget>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QPalette>
#include <QColorDialog>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSettings>

#include "appearancesettingspage.h"
#include "core/appearance.h"
#include "core/iconloader.h"
#include "core/stylehelper.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "settingspage.h"
#include "settingsdialog.h"
#include "ui_appearancesettingspage.h"

const char *AppearanceSettingsPage::kSettingsGroup = "Appearance";

const char *AppearanceSettingsPage::kUseCustomColorSet = "use-custom-set";
const char *AppearanceSettingsPage::kForegroundColor = "foreground-color";
const char *AppearanceSettingsPage::kBackgroundColor = "background-color";

const char *AppearanceSettingsPage::kBackgroundImageType = "background_image_type";
const char *AppearanceSettingsPage::kBackgroundImageFilename = "background_image_file";
const char *AppearanceSettingsPage::kBackgroundImagePosition = "background_image_position";
const char *AppearanceSettingsPage::kBackgroundImageStretch = "background_image_stretch";
const char *AppearanceSettingsPage::kBackgroundImageDoNotCut = "background_image_do_not_cut";
const char *AppearanceSettingsPage::kBackgroundImageKeepAspectRatio = "background_image_keep_aspect_ratio";
const char *AppearanceSettingsPage::kBackgroundImageMaxSize = "background_image_max_size";

const char *AppearanceSettingsPage::kBlurRadius = "blur_radius";
const char *AppearanceSettingsPage::kOpacityLevel = "opacity_level";

const int AppearanceSettingsPage::kDefaultBlurRadius = 0;
const int AppearanceSettingsPage::kDefaultOpacityLevel = 40;

const char *AppearanceSettingsPage::kSystemThemeIcons = "system_icons";

const char *AppearanceSettingsPage::kTabBarSystemColor= "tab_system_color";
const char *AppearanceSettingsPage::kTabBarGradient = "tab_gradient";
const char *AppearanceSettingsPage::kTabBarColor = "tab_color";

AppearanceSettingsPage::AppearanceSettingsPage(SettingsDialog *dialog)
    : SettingsPage(dialog),
      ui_(new Ui_AppearanceSettingsPage),
      original_use_a_custom_color_set_(false),
      background_image_type_(BackgroundImageType_Default) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("view-media-visualization"));

  ui_->combobox_backgroundimageposition->setItemData(0, BackgroundImagePosition_UpperLeft);
  ui_->combobox_backgroundimageposition->setItemData(1, BackgroundImagePosition_UpperRight);
  ui_->combobox_backgroundimageposition->setItemData(2, BackgroundImagePosition_Middle);
  ui_->combobox_backgroundimageposition->setItemData(3, BackgroundImagePosition_BottomLeft);
  ui_->combobox_backgroundimageposition->setItemData(4, BackgroundImagePosition_BottomRight);

  connect(ui_->blur_slider, SIGNAL(valueChanged(int)), SLOT(BlurLevelChanged(int)));
  connect(ui_->opacity_slider, SIGNAL(valueChanged(int)), SLOT(OpacityLevelChanged(int)));

  connect(ui_->use_a_custom_color_set, SIGNAL(toggled(bool)), SLOT(UseCustomColorSetOptionChanged(bool)));
  connect(ui_->select_foreground_color, SIGNAL(pressed()), SLOT(SelectForegroundColor()));
  connect(ui_->select_background_color, SIGNAL(pressed()), SLOT(SelectBackgroundColor()));

  connect(ui_->use_default_background, SIGNAL(toggled(bool)), ui_->widget_custom_background_image_options, SLOT(setDisabled(bool)));
  connect(ui_->use_no_background, SIGNAL(toggled(bool)), ui_->widget_custom_background_image_options, SLOT(setDisabled(bool)));
  connect(ui_->use_custom_background_image, SIGNAL(toggled(bool)), ui_->widget_custom_background_image_options, SLOT(setEnabled(bool)));
  connect(ui_->use_album_cover_background, SIGNAL(toggled(bool)), ui_->widget_custom_background_image_options, SLOT(setEnabled(bool)));

  connect(ui_->select_background_image_filename_button, SIGNAL(pressed()), SLOT(SelectBackgroundImage()));
  connect(ui_->use_custom_background_image, SIGNAL(toggled(bool)), ui_->background_image_filename, SLOT(setEnabled(bool)));
  connect(ui_->use_custom_background_image, SIGNAL(toggled(bool)), ui_->select_background_image_filename_button, SLOT(setEnabled(bool)));

  connect(ui_->checkbox_background_image_stretch, SIGNAL(toggled(bool)), ui_->checkbox_background_image_do_not_cut, SLOT(setEnabled(bool)));
  connect(ui_->checkbox_background_image_stretch, SIGNAL(toggled(bool)), ui_->checkbox_background_image_keep_aspect_ratio, SLOT(setEnabled(bool)));
  connect(ui_->checkbox_background_image_stretch, SIGNAL(toggled(bool)), ui_->spinbox_background_image_maxsize, SLOT(setDisabled(bool)));

  connect(ui_->checkbox_background_image_keep_aspect_ratio, SIGNAL(toggled(bool)), ui_->checkbox_background_image_do_not_cut, SLOT(setEnabled(bool)));

  connect(ui_->select_tabbar_color, SIGNAL(pressed()), SLOT(TabBarSelectBGColor()));
  connect(ui_->tabbar_system_color, SIGNAL(toggled(bool)), SLOT(TabBarSystemColor(bool)));

  Load();

}

AppearanceSettingsPage::~AppearanceSettingsPage() {
  delete ui_;
}

void AppearanceSettingsPage::Load() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  QPalette p = QApplication::palette();

  // Keep in mind originals colors, in case the user clicks on Cancel, to be able to restore colors
  original_use_a_custom_color_set_ = s.value(kUseCustomColorSet, false).toBool();

  original_foreground_color_  = s.value(kForegroundColor, p.color(QPalette::WindowText)).value<QColor>();
  current_foreground_color_ = original_foreground_color_;
  original_background_color_  = s.value(kBackgroundColor, p.color(QPalette::Window)).value<QColor>();
  current_background_color_ = original_background_color_;

  InitColorSelectorsColors();

  // Tab widget BG color settings.
  bool tabbar_system_color = s.value(kTabBarSystemColor, true).toBool();
  ui_->tabbar_gradient->setChecked(s.value(kTabBarGradient, true).toBool());
  ui_->tabbar_system_color->setChecked(tabbar_system_color);
  ui_->tabbar_custom_color->setChecked(!tabbar_system_color);

  current_tabbar_bg_color_ = s.value(kTabBarColor, StyleHelper::highlightColor()).value<QColor>();

  UpdateColorSelectorColor(ui_->select_tabbar_color, current_tabbar_bg_color_);
  TabBarSystemColor(ui_->tabbar_system_color->isChecked());

  // Playlist settings
  background_image_type_ = static_cast<BackgroundImageType>(s.value(kBackgroundImageType).toInt());
  background_image_filename_ = s.value(kBackgroundImageFilename).toString();

  ui_->use_system_color_set->setChecked(!original_use_a_custom_color_set_);
  ui_->use_a_custom_color_set->setChecked(original_use_a_custom_color_set_);

  switch (background_image_type_) {
    case BackgroundImageType_None:
      ui_->use_no_background->setChecked(true);
      break;
    case BackgroundImageType_Album:
      ui_->use_album_cover_background->setChecked(true);
      break;
    case BackgroundImageType_Custom:
      ui_->use_custom_background_image->setChecked(true);
      break;
    case BackgroundImageType_Default:
    default:
      ui_->use_default_background->setChecked(true);
  }
  ui_->background_image_filename->setText(background_image_filename_);

  ui_->combobox_backgroundimageposition->setCurrentIndex(ui_->combobox_backgroundimageposition->findData(s.value(kBackgroundImagePosition, BackgroundImagePosition_BottomRight).toInt()));
  ui_->spinbox_background_image_maxsize->setValue(s.value(kBackgroundImageMaxSize, 0).toInt());
  ui_->checkbox_background_image_stretch->setChecked(s.value(kBackgroundImageStretch, false).toBool());
  ui_->checkbox_background_image_do_not_cut->setChecked(s.value(kBackgroundImageDoNotCut, true).toBool());
  ui_->checkbox_background_image_keep_aspect_ratio->setChecked(s.value(kBackgroundImageKeepAspectRatio, true).toBool());
  ui_->blur_slider->setValue(s.value(kBlurRadius, kDefaultBlurRadius).toInt());
  ui_->opacity_slider->setValue(s.value(kOpacityLevel, kDefaultOpacityLevel).toInt());
  ui_->checkbox_system_icons->setChecked(s.value(kSystemThemeIcons, false).toBool());

  ui_->checkbox_background_image_keep_aspect_ratio->setEnabled(ui_->checkbox_background_image_stretch->isChecked());
  ui_->checkbox_background_image_do_not_cut->setEnabled(ui_->checkbox_background_image_stretch->isChecked() && ui_->checkbox_background_image_keep_aspect_ratio->isChecked());

  s.endGroup();

  Init(ui_->layout_appearancesettingspage->parentWidget());

}

void AppearanceSettingsPage::Save() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  bool use_a_custom_color_set = ui_->use_a_custom_color_set->isChecked();
  s.setValue(kUseCustomColorSet, use_a_custom_color_set);
  if (use_a_custom_color_set) {
    s.setValue(kBackgroundColor, current_background_color_);
    s.setValue(kForegroundColor, current_foreground_color_);
  }
  else {
    dialog()->appearance()->ResetToSystemDefaultTheme();
    s.remove(kBackgroundColor);
    s.remove(kForegroundColor);
  }

  background_image_filename_ = ui_->background_image_filename->text();
  if (ui_->use_no_background->isChecked()) {
    background_image_type_ = BackgroundImageType_None;
  }
  else if (ui_->use_album_cover_background->isChecked()) {
    background_image_type_ = BackgroundImageType_Album;
  }
  else if (ui_->use_default_background->isChecked()) {
    background_image_type_ = BackgroundImageType_Default;
  }
  else if (ui_->use_custom_background_image->isChecked()) {
    background_image_type_ = BackgroundImageType_Custom;
  }
  s.setValue(kBackgroundImageType, background_image_type_);

  if (background_image_type_ == BackgroundImageType_Custom)
      s.setValue(kBackgroundImageFilename, background_image_filename_);
  else
    s.remove(kBackgroundImageFilename);

  BackgroundImagePosition backgroundimageposition = BackgroundImagePosition(ui_->combobox_backgroundimageposition->itemData(ui_->combobox_backgroundimageposition->currentIndex()).toInt());
  s.setValue(kBackgroundImageMaxSize, ui_->spinbox_background_image_maxsize->value());
  s.setValue(kBackgroundImagePosition, backgroundimageposition);
  s.setValue(kBackgroundImageStretch, ui_->checkbox_background_image_stretch->isChecked());
  s.setValue(kBackgroundImageDoNotCut, ui_->checkbox_background_image_do_not_cut->isChecked());
  s.setValue(kBackgroundImageKeepAspectRatio, ui_->checkbox_background_image_keep_aspect_ratio->isChecked());

  s.setValue(kBlurRadius, ui_->blur_slider->value());
  s.setValue(kOpacityLevel, ui_->opacity_slider->value());

  s.setValue(kSystemThemeIcons, ui_->checkbox_system_icons->isChecked());

  s.setValue(kTabBarSystemColor, ui_->tabbar_system_color->isChecked());
  s.setValue(kTabBarGradient, ui_->tabbar_gradient->isChecked());
  s.setValue(kTabBarColor, current_tabbar_bg_color_);

  s.endGroup();

}

void AppearanceSettingsPage::Cancel() {

  if (original_use_a_custom_color_set_) {
    dialog()->appearance()->ChangeForegroundColor(original_foreground_color_);
    dialog()->appearance()->ChangeBackgroundColor(original_background_color_);
  }
  else {
    dialog()->appearance()->ResetToSystemDefaultTheme();
  }

}

void AppearanceSettingsPage::SelectForegroundColor() {

  QColor color_selected = QColorDialog::getColor(current_foreground_color_);
  if (!color_selected.isValid()) return;

  current_foreground_color_ = color_selected;
  dialog()->appearance()->ChangeForegroundColor(color_selected);

  UpdateColorSelectorColor(ui_->select_foreground_color, color_selected);

  set_changed();

}

void AppearanceSettingsPage::SelectBackgroundColor() {

  QColor color_selected = QColorDialog::getColor(current_background_color_);
  if (!color_selected.isValid()) return;

  current_background_color_ = color_selected;
  dialog()->appearance()->ChangeBackgroundColor(color_selected);

  UpdateColorSelectorColor(ui_->select_background_color, color_selected);

  set_changed();

}

void AppearanceSettingsPage::UseCustomColorSetOptionChanged(bool checked) {

  if (checked) {
    dialog()->appearance()->ChangeForegroundColor(current_foreground_color_);
    dialog()->appearance()->ChangeBackgroundColor(current_background_color_);
  }
  else {
    dialog()->appearance()->ResetToSystemDefaultTheme();
    QPalette p = QApplication::palette();
    current_foreground_color_ = p.color(QPalette::WindowText);
    current_background_color_ = p.color(QPalette::Window);
    UpdateColorSelectorColor(ui_->select_foreground_color, current_foreground_color_);
    UpdateColorSelectorColor(ui_->select_background_color, current_background_color_);
  }

}

void AppearanceSettingsPage::InitColorSelectorsColors() {

  UpdateColorSelectorColor(ui_->select_foreground_color, current_foreground_color_);
  UpdateColorSelectorColor(ui_->select_background_color, current_background_color_);

}

void AppearanceSettingsPage::UpdateColorSelectorColor(QWidget *color_selector, const QColor &color) {

  QString css = QString("background-color: rgb(%1, %2, %3); color: rgb(255, 255, 255); border: 1px dotted black;").arg(color.red()).arg(color.green()).arg(color.blue());
  color_selector->setStyleSheet(css);

}

void AppearanceSettingsPage::SelectBackgroundImage() {

  QString selected_filename = QFileDialog::getOpenFileName(this, tr("Select background image"), background_image_filename_, tr(AlbumCoverChoiceController::kLoadImageFileFilter) + ";;" + tr(AlbumCoverChoiceController::kAllFilesFilter));
  if (selected_filename.isEmpty()) return;
  background_image_filename_ = selected_filename;
  ui_->background_image_filename->setText(background_image_filename_);

}

void AppearanceSettingsPage::BlurLevelChanged(int value) {
  ui_->background_blur_radius_label->setText(QString("%1px").arg(value));
}

void AppearanceSettingsPage::OpacityLevelChanged(int percent) {
  ui_->background_opacity_label->setText(QString("%1%").arg(percent));
}

void AppearanceSettingsPage::TabBarSystemColor(bool checked) {

  if (checked) {
    current_tabbar_bg_color_ = StyleHelper::highlightColor();
    UpdateColorSelectorColor(ui_->select_tabbar_color, current_tabbar_bg_color_);
  }
  ui_->layout_tabbar_color->setEnabled(!checked);
  ui_->select_tabbar_color->setEnabled(!checked);

}

void AppearanceSettingsPage::TabBarSelectBGColor() {

  if (ui_->tabbar_system_color->isChecked()) return;

  QColor color_selected = QColorDialog::getColor(current_tabbar_bg_color_);
  if (!color_selected.isValid()) return;
  current_tabbar_bg_color_ = color_selected;
  UpdateColorSelectorColor(ui_->select_tabbar_color, current_tabbar_bg_color_);

}
