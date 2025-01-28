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

#include "config.h"

#include <utility>

#include <QApplication>
#include <QWidget>
#include <QStyleFactory>
#include <QVariant>
#include <QString>
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
#include "constants/appearancesettings.h"
#include "constants/filefilterconstants.h"
#include "core/iconloader.h"
#include "core/stylehelper.h"
#include "core/settings.h"
#include "widgets/fancytabwidget.h"
#include "settingspage.h"
#include "settingsdialog.h"
#include "ui_appearancesettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace AppearanceSettings;

AppearanceSettingsPage::AppearanceSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_AppearanceSettingsPage),
      background_image_type_(BackgroundImageType::Default) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"view-media-visualization"_s, true, 0, 32));

  ui_->combobox_style->addItem(u"default"_s, u"default"_s);
  const QStringList styles = QStyleFactory::keys();
  for (const QString &style : styles) {
    ui_->combobox_style->addItem(style, style);
  }

  ui_->combobox_backgroundimageposition->setItemData(0, static_cast<int>(BackgroundImagePosition::UpperLeft));
  ui_->combobox_backgroundimageposition->setItemData(1, static_cast<int>(BackgroundImagePosition::UpperRight));
  ui_->combobox_backgroundimageposition->setItemData(2, static_cast<int>(BackgroundImagePosition::Middle));
  ui_->combobox_backgroundimageposition->setItemData(3, static_cast<int>(BackgroundImagePosition::BottomLeft));
  ui_->combobox_backgroundimageposition->setItemData(4, static_cast<int>(BackgroundImagePosition::BottomRight));

  QObject::connect(ui_->blur_slider, &QSlider::valueChanged, this, &AppearanceSettingsPage::BlurLevelChanged);
  QObject::connect(ui_->opacity_slider, &QSlider::valueChanged, this, &AppearanceSettingsPage::OpacityLevelChanged);

  QObject::connect(ui_->use_default_background, &QRadioButton::toggled, ui_->widget_custom_background_image_options, &AppearanceSettingsPage::setDisabled);
  QObject::connect(ui_->use_no_background, &QRadioButton::toggled, ui_->widget_custom_background_image_options, &AppearanceSettingsPage::setDisabled);
  QObject::connect(ui_->use_album_cover_background, &QRadioButton::toggled, ui_->widget_custom_background_image_options, &AppearanceSettingsPage::setEnabled);
  QObject::connect(ui_->use_strawbs_background, &QRadioButton::toggled, ui_->widget_custom_background_image_options, &AppearanceSettingsPage::setDisabled);
  QObject::connect(ui_->use_custom_background_image, &QRadioButton::toggled, ui_->widget_custom_background_image_options, &AppearanceSettingsPage::setEnabled);

  QObject::connect(ui_->select_background_image_filename_button, &QPushButton::pressed, this, &AppearanceSettingsPage::SelectBackgroundImage);
  QObject::connect(ui_->use_custom_background_image, &QRadioButton::toggled, ui_->background_image_filename, &AppearanceSettingsPage::setEnabled);
  QObject::connect(ui_->use_custom_background_image, &QRadioButton::toggled, ui_->select_background_image_filename_button, &AppearanceSettingsPage::setEnabled);

  QObject::connect(ui_->checkbox_background_image_stretch, &QCheckBox::toggled, ui_->checkbox_background_image_do_not_cut, &AppearanceSettingsPage::setEnabled);
  QObject::connect(ui_->checkbox_background_image_stretch, &QCheckBox::toggled, ui_->checkbox_background_image_keep_aspect_ratio, &AppearanceSettingsPage::setEnabled);
  QObject::connect(ui_->checkbox_background_image_stretch, &QCheckBox::toggled, ui_->spinbox_background_image_maxsize, &AppearanceSettingsPage::setDisabled);

  QObject::connect(ui_->checkbox_background_image_keep_aspect_ratio, &QCheckBox::toggled, ui_->checkbox_background_image_do_not_cut, &AppearanceSettingsPage::setEnabled);

  QObject::connect(ui_->select_tabbar_color, &QPushButton::pressed, this, &AppearanceSettingsPage::TabBarSelectBGColor);
  QObject::connect(ui_->tabbar_system_color, &QRadioButton::toggled, this, &AppearanceSettingsPage::TabBarSystemColor);

  QObject::connect(ui_->select_playlist_playing_song_color, &QPushButton::pressed, this, &AppearanceSettingsPage::PlaylistPlayingSongSelectColor);
  QObject::connect(ui_->playlist_playing_song_color_system, &QRadioButton::toggled, this, &AppearanceSettingsPage::PlaylistPlayingSongColorSystem);

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  ui_->checkbox_system_icons->hide();
#endif

  AppearanceSettingsPage::Load();

}

AppearanceSettingsPage::~AppearanceSettingsPage() {
  delete ui_;
}

void AppearanceSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  ComboBoxLoadFromSettings(s, ui_->combobox_style, QLatin1String(kStyle), u"default"_s);

#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN32)
  ui_->checkbox_system_icons->setChecked(s.value(kSystemThemeIcons, false).toBool());
#endif

  // Tab widget BG color settings.
  bool tabbar_system_color = s.value(kTabBarSystemColor, true).toBool();
  ui_->tabbar_gradient->setChecked(s.value(kTabBarGradient, true).toBool());
  ui_->tabbar_system_color->setChecked(tabbar_system_color);
  ui_->tabbar_custom_color->setChecked(!tabbar_system_color);

  current_tabbar_bg_color_ = s.value(kTabBarColor, FancyTabWidget::DefaultTabbarBgColor()).value<QColor>();

  UpdateColorSelectorColor(ui_->select_tabbar_color, current_tabbar_bg_color_);
  TabBarSystemColor(ui_->tabbar_system_color->isChecked());

  // Playlist settings
  background_image_type_ = static_cast<BackgroundImageType>(s.value(kBackgroundImageType, static_cast<int>(BackgroundImageType::Default)).toInt());
  background_image_filename_ = s.value(kBackgroundImageFilename).toString();

  switch (background_image_type_) {
    case BackgroundImageType::Default:
      ui_->use_default_background->setChecked(true);
      break;
    case BackgroundImageType::None:
      ui_->use_no_background->setChecked(true);
      break;
    case BackgroundImageType::Album:
      ui_->use_album_cover_background->setChecked(true);
      break;
    case BackgroundImageType::Strawbs:
      ui_->use_strawbs_background->setChecked(true);
      break;
    case BackgroundImageType::Custom:
      ui_->use_custom_background_image->setChecked(true);
      break;
  }
  ui_->background_image_filename->setText(background_image_filename_);

  ui_->combobox_backgroundimageposition->setCurrentIndex(ui_->combobox_backgroundimageposition->findData(s.value(kBackgroundImagePosition, static_cast<int>(BackgroundImagePosition::BottomRight)).toInt()));
  ui_->spinbox_background_image_maxsize->setValue(s.value(kBackgroundImageMaxSize, 0).toInt());
  ui_->checkbox_background_image_stretch->setChecked(s.value(kBackgroundImageStretch, false).toBool());
  ui_->checkbox_background_image_do_not_cut->setChecked(s.value(kBackgroundImageDoNotCut, true).toBool());
  ui_->checkbox_background_image_keep_aspect_ratio->setChecked(s.value(kBackgroundImageKeepAspectRatio, true).toBool());
  ui_->blur_slider->setValue(s.value(kBlurRadius, kDefaultBlurRadius).toInt());
  ui_->opacity_slider->setValue(s.value(kOpacityLevel, kDefaultOpacityLevel).toInt());

  ui_->checkbox_background_image_keep_aspect_ratio->setEnabled(ui_->checkbox_background_image_stretch->isChecked());
  ui_->checkbox_background_image_do_not_cut->setEnabled(ui_->checkbox_background_image_stretch->isChecked() && ui_->checkbox_background_image_keep_aspect_ratio->isChecked());

  ui_->spinbox_icon_size_tabbar_small_mode->setValue(s.value(kIconSizeTabbarSmallMode, 32).toInt());
  ui_->spinbox_icon_size_tabbar_large_mode->setValue(s.value(kIconSizeTabbarLargeMode, 40).toInt());
  ui_->spinbox_icon_size_play_control_buttons->setValue(s.value(kIconSizePlayControlButtons, 32).toInt());
  ui_->spinbox_icon_size_playlist_buttons->setValue(s.value(kIconSizePlaylistButtons, 20).toInt());
  ui_->spinbox_icon_size_left_panel_buttons->setValue(s.value(kIconSizeLeftPanelButtons, 22).toInt());
  ui_->spinbox_icon_size_configure_buttons->setValue(s.value(kIconSizeConfigureButtons, 16).toInt());

  current_playlist_playing_song_color_ = s.value(kPlaylistPlayingSongColor).value<QColor>();
  if (current_playlist_playing_song_color_.isValid()) {
    ui_->playlist_playing_song_color_custom->setChecked(true);
  }
  else {
    ui_->playlist_playing_song_color_system->setChecked(true);
    current_playlist_playing_song_color_ = StyleHelper::highlightColor();
  }
  UpdateColorSelectorColor(ui_->select_playlist_playing_song_color, current_playlist_playing_song_color_);
  PlaylistPlayingSongColorSystem(ui_->playlist_playing_song_color_system->isChecked());

  s.endGroup();

  Init(ui_->layout_appearancesettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void AppearanceSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  s.setValue("style", ui_->combobox_style->currentText());

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  s.setValue(kSystemThemeIcons, false);
#else
  s.setValue(kSystemThemeIcons, ui_->checkbox_system_icons->isChecked());
#endif

  background_image_filename_ = ui_->background_image_filename->text();
  if (ui_->use_default_background->isChecked()) {
    background_image_type_ = BackgroundImageType::Default;
  }
  else if (ui_->use_no_background->isChecked()) {
    background_image_type_ = BackgroundImageType::None;
  }
  else if (ui_->use_album_cover_background->isChecked()) {
    background_image_type_ = BackgroundImageType::Album;
  }
  else if (ui_->use_strawbs_background->isChecked()) {
    background_image_type_ = BackgroundImageType::Strawbs;
  }
  else if (ui_->use_custom_background_image->isChecked()) {
    background_image_type_ = BackgroundImageType::Custom;
  }
  s.setValue(kBackgroundImageType, static_cast<int>(background_image_type_));

  if (background_image_type_ == BackgroundImageType::Custom) {
    s.setValue(kBackgroundImageFilename, background_image_filename_);
  }
  else {
    s.remove(kBackgroundImageFilename);
  }

  s.setValue(kBackgroundImageMaxSize, ui_->spinbox_background_image_maxsize->value());
  s.setValue(kBackgroundImagePosition, ui_->combobox_backgroundimageposition->currentData().toInt());
  s.setValue(kBackgroundImageStretch, ui_->checkbox_background_image_stretch->isChecked());
  s.setValue(kBackgroundImageDoNotCut, ui_->checkbox_background_image_do_not_cut->isChecked());
  s.setValue(kBackgroundImageKeepAspectRatio, ui_->checkbox_background_image_keep_aspect_ratio->isChecked());

  s.setValue(kBlurRadius, ui_->blur_slider->value());
  s.setValue(kOpacityLevel, ui_->opacity_slider->value());

  s.setValue(kTabBarSystemColor, ui_->tabbar_system_color->isChecked());
  s.setValue(kTabBarGradient, ui_->tabbar_gradient->isChecked());
  s.setValue(kTabBarColor, current_tabbar_bg_color_);

  s.setValue(kIconSizeTabbarSmallMode, ui_->spinbox_icon_size_tabbar_small_mode->value());
  s.setValue(kIconSizeTabbarLargeMode, ui_->spinbox_icon_size_tabbar_large_mode->value());
  s.setValue(kIconSizePlayControlButtons, ui_->spinbox_icon_size_play_control_buttons->value());
  s.setValue(kIconSizePlaylistButtons, ui_->spinbox_icon_size_playlist_buttons->value());
  s.setValue(kIconSizeLeftPanelButtons, ui_->spinbox_icon_size_left_panel_buttons->value());
  s.setValue(kIconSizeConfigureButtons, ui_->spinbox_icon_size_configure_buttons->value());

  if (ui_->playlist_playing_song_color_system->isChecked()) {
    s.setValue(kPlaylistPlayingSongColor, QColor());
  }
  else {
    s.setValue(kPlaylistPlayingSongColor, current_playlist_playing_song_color_);
  }

  s.endGroup();

}

void AppearanceSettingsPage::UpdateColorSelectorColor(QWidget *color_selector, const QColor &color) {

  QString css = QStringLiteral("background-color: rgb(%1, %2, %3); color: rgb(255, 255, 255); border: 1px dotted black;").arg(color.red()).arg(color.green()).arg(color.blue());
  color_selector->setStyleSheet(css);

}

void AppearanceSettingsPage::SelectBackgroundImage() {

  QString selected_filename = QFileDialog::getOpenFileName(this, tr("Select background image"), background_image_filename_, tr(kLoadImageFileFilter) + u";;"_s + tr(kAllFilesFilterSpec));
  if (selected_filename.isEmpty()) return;
  background_image_filename_ = selected_filename;
  ui_->background_image_filename->setText(background_image_filename_);

}

void AppearanceSettingsPage::BlurLevelChanged(int value) {
  ui_->background_blur_radius_label->setText(QStringLiteral("%1px").arg(value));
}

void AppearanceSettingsPage::OpacityLevelChanged(int percent) {
  ui_->background_opacity_label->setText(QStringLiteral("%1%").arg(percent));
}

void AppearanceSettingsPage::TabBarSystemColor(bool checked) {

  if (checked) {
    current_tabbar_bg_color_ = FancyTabWidget::DefaultTabbarBgColor();
    UpdateColorSelectorColor(ui_->select_tabbar_color, current_tabbar_bg_color_);
  }
  ui_->layout_tabbar_color->setEnabled(!checked);
  ui_->select_tabbar_color->setEnabled(!checked);

  set_changed();

}

void AppearanceSettingsPage::TabBarSelectBGColor() {

  if (ui_->tabbar_system_color->isChecked()) return;

  QColor color_selected = QColorDialog::getColor(current_tabbar_bg_color_);
  if (!color_selected.isValid()) return;
  current_tabbar_bg_color_ = color_selected;
  UpdateColorSelectorColor(ui_->select_tabbar_color, current_tabbar_bg_color_);

  set_changed();

}

void AppearanceSettingsPage::PlaylistPlayingSongColorSystem(bool checked) {

  if (checked) {
    current_playlist_playing_song_color_ = StyleHelper::highlightColor();
    UpdateColorSelectorColor(ui_->select_playlist_playing_song_color, current_playlist_playing_song_color_);
  }
  ui_->layout_playlist_playing_song_color_custom->setEnabled(!checked);
  ui_->select_playlist_playing_song_color->setEnabled(!checked);

  set_changed();

}

void AppearanceSettingsPage::PlaylistPlayingSongSelectColor() {

  if (ui_->playlist_playing_song_color_system->isChecked()) return;

  QColor color_selected = QColorDialog::getColor(current_playlist_playing_song_color_);
  if (!color_selected.isValid()) return;
  current_playlist_playing_song_color_ = color_selected;
  UpdateColorSelectorColor(ui_->select_playlist_playing_song_color, current_playlist_playing_song_color_);

  set_changed();

}

