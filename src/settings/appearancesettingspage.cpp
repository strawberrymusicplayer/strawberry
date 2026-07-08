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

#include "config.h"

#include <utility>

#include <QApplication>
#include <QGuiApplication>
#include <QWidget>
#include <QStyleFactory>
#include <QStyleHints>
#include <QVariant>
#include <QMap>
#include <QString>
#include <QColor>
#include <QPalette>
#include <QColorDialog>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QBoxLayout>
#include <QFormLayout>
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
#include "core/appearance.h"
#include "widgets/fancytabwidget.h"
#include "settingspage.h"
#include "settingsdialog.h"
#include "ui_appearancesettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace AppearanceSettings;

AppearanceSettingsPage::AppearanceSettingsPage(SettingsDialog *dialog, SharedPtr<Appearance> appearance, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_AppearanceSettingsPage),
      appearance_(appearance),
      original_dark_mode_(false),
      original_use_custom_color_set_(false),
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

  QObject::connect(ui_->checkbox_dark_mode, &QCheckBox::toggled, this, &AppearanceSettingsPage::DarkModeToggled);

  QObject::connect(ui_->blur_slider, &QSlider::valueChanged, this, &AppearanceSettingsPage::BlurLevelChanged);
  QObject::connect(ui_->opacity_slider, &QSlider::valueChanged, this, &AppearanceSettingsPage::OpacityLevelChanged);

  QObject::connect(ui_->use_custom_color_set, &QRadioButton::toggled, this, &AppearanceSettingsPage::UseCustomColorSetOptionChanged);
  QObject::connect(ui_->use_custom_color_set, &QRadioButton::toggled, ui_->widget_custom_colors, &QWidget::setEnabled);
  QObject::connect(ui_->button_dark_colors, &QPushButton::pressed, this, &AppearanceSettingsPage::SetDarkColors);
  QObject::connect(ui_->button_reset_colors, &QPushButton::pressed, this, &AppearanceSettingsPage::ResetToDefaultColors);

  CreateColorSelectors();

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

  // Disconnect while loading so intermediate combobox signals don't trigger StyleChanged before colors are ready.
  QObject::disconnect(ui_->combobox_style, &QComboBox::currentIndexChanged, this, &AppearanceSettingsPage::StyleChanged);

  Settings s;
  s.beginGroup(kSettingsGroup);

  ComboBoxLoadFromSettings(s, ui_->combobox_style, QLatin1String(kStyle), u"default"_s);

#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN32)
  ui_->checkbox_system_icons->setChecked(s.value(kSystemThemeIcons, kDefaultSystemThemeIcons).toBool());
#endif

  const QPalette p = QApplication::palette();

  // Keep in mind originals colors, in case the user clicks on Cancel, to be able to restore colors
  original_use_custom_color_set_ = s.value(kUseCustomColorSet, kDefaultUseCustomColorSet).toBool();

  current_colors_.clear();
  for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
    const QVariant value = s.value(color_role.settings_key);
    QColor color = value.isValid() ? value.value<QColor>() : QColor();
    if (!color.isValid()) {
      color = p.color(color_role.role);
    }
    current_colors_.insert(color_role.role, color);
  }

  original_colors_ = current_colors_;

  UpdateColorSelectorsColors();

  // Tab widget BG color settings.
  bool tabbar_system_color = s.value(kTabBarSystemColor, kDefaultTabBarSystemColor).toBool();
  ui_->tabbar_gradient->setChecked(s.value(kTabBarGradient, kDefaultTabBarGradient).toBool());
  ui_->tabbar_system_color->setChecked(tabbar_system_color);
  ui_->tabbar_custom_color->setChecked(!tabbar_system_color);

  current_tabbar_bg_color_ = s.value(kTabBarColor, FancyTabWidget::DefaultTabbarBgColor()).value<QColor>();

  UpdateColorSelectorColor(ui_->select_tabbar_color, current_tabbar_bg_color_);
  TabBarSystemColor(ui_->tabbar_system_color->isChecked());

  // Playlist settings
  background_image_type_ = static_cast<BackgroundImageType>(s.value(kBackgroundImageType, static_cast<int>(kDefaultBackgroundImageType)).toInt());
  background_image_filename_ = s.value(kBackgroundImageFilename).toString();

  ui_->use_system_color_set->setChecked(!original_use_custom_color_set_);
  ui_->use_custom_color_set->setChecked(original_use_custom_color_set_);
  ui_->widget_custom_colors->setEnabled(original_use_custom_color_set_);

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

  ui_->combobox_backgroundimageposition->setCurrentIndex(ui_->combobox_backgroundimageposition->findData(s.value(kBackgroundImagePosition, static_cast<int>(kDefaultBackgroundImagePosition)).toInt()));
  ui_->spinbox_background_image_maxsize->setValue(s.value(kBackgroundImageMaxSize, kDefaultBackgroundImageMaxSize).toInt());
  ui_->checkbox_background_image_stretch->setChecked(s.value(kBackgroundImageStretch, kDefaultBackgroundImageStretch).toBool());
  ui_->checkbox_background_image_do_not_cut->setChecked(s.value(kBackgroundImageDoNotCut, kDefaultBackgroundImageDoNotCut).toBool());
  ui_->checkbox_background_image_keep_aspect_ratio->setChecked(s.value(kBackgroundImageKeepAspectRatio, kDefaultBackgroundImageKeepAspectRatio).toBool());
  ui_->blur_slider->setValue(s.value(kBlurRadius, kDefaultBlurRadius).toInt());
  ui_->opacity_slider->setValue(s.value(kOpacityLevel, kDefaultOpacityLevel).toInt());

  ui_->checkbox_background_image_keep_aspect_ratio->setEnabled(ui_->checkbox_background_image_stretch->isChecked());
  ui_->checkbox_background_image_do_not_cut->setEnabled(ui_->checkbox_background_image_stretch->isChecked() && ui_->checkbox_background_image_keep_aspect_ratio->isChecked());

  ui_->spinbox_icon_size_tabbar_small_mode->setValue(s.value(kIconSizeTabbarSmallMode, kDefaultIconSizeTabbarSmallMode).toInt());
  ui_->spinbox_icon_size_tabbar_large_mode->setValue(s.value(kIconSizeTabbarLargeMode, kDefaultIconSizeTabbarLargeMode).toInt());
  ui_->spinbox_icon_size_play_control_buttons->setValue(s.value(kIconSizePlayControlButtons, kDefaultIconSizePlayControlButtons).toInt());
  ui_->spinbox_icon_size_playlist_buttons->setValue(s.value(kIconSizePlaylistButtons, kDefaultIconSizePlaylistButtons).toInt());
  ui_->spinbox_icon_size_left_panel_buttons->setValue(s.value(kIconSizeLeftPanelButtons, kDefaultIconSizeLeftPanelButtons).toInt());
  ui_->spinbox_icon_size_configure_buttons->setValue(s.value(kIconSizeConfigureButtons, kDefaultIconSizeConfigureButtons).toInt());

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

  original_dark_mode_ = s.value(kDarkMode, kDefaultDarkMode).toBool();
  ui_->checkbox_dark_mode->setChecked(original_dark_mode_);

  s.endGroup();

  Init(ui_->layout_appearancesettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

  QObject::connect(ui_->combobox_style, &QComboBox::currentIndexChanged, this, &AppearanceSettingsPage::StyleChanged);
  StyleChanged(ui_->combobox_style->currentIndex());

}

void AppearanceSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  s.setValue(kStyle, ui_->combobox_style->currentText());

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  s.setValue(kSystemThemeIcons, false);
#else
  s.setValue(kSystemThemeIcons, ui_->checkbox_system_icons->isChecked());
#endif

  if (IsNativeStyle(ui_->combobox_style->currentData().toString())) {
    s.setValue(kDarkMode, ui_->checkbox_dark_mode->isChecked());
    s.setValue(kUseCustomColorSet, false);
  }
  else if (IsBreezeStyle()) {
    s.setValue(kDarkMode, false);
    s.setValue(kUseCustomColorSet, false);
  }
  else {
    s.setValue(kDarkMode, false);
    const bool use_custom_color_set = ui_->use_custom_color_set->isChecked();
    s.setValue(kUseCustomColorSet, use_custom_color_set);
    if (use_custom_color_set) {
      for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
        s.setValue(color_role.settings_key, current_colors_.value(color_role.role));
      }
    }
    else {
      appearance_->ResetToSystemDefaultTheme();
      for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
        s.remove(color_role.settings_key);
      }
    }
  }

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

void AppearanceSettingsPage::Cancel() {

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  QGuiApplication::styleHints()->setColorScheme(original_dark_mode_ ? Qt::ColorScheme::Dark : Qt::ColorScheme::Unknown);
#endif

  if (original_use_custom_color_set_) {
    Appearance::ChangeColors(original_colors_);
  }
  else {
    appearance_->ResetToSystemDefaultTheme();
  }

}

void AppearanceSettingsPage::StyleChanged(const int index) {

  const QString style_name = ui_->combobox_style->itemData(index).toString();
  const bool is_native = IsNativeStyle(style_name);
  const bool is_breeze = IsBreezeStyle();

  ui_->checkbox_dark_mode->setVisible(is_native);
  ui_->groupbox_colors->setEnabled(!is_native && !is_breeze);
  ui_->groupbox_colors->setToolTip(is_breeze ? tr("Custom colors are not supported with the Breeze style, because Breeze colors the menubar and toolbars from the KDE color scheme instead of the application palette.") : QString());

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  if (is_native) {
    appearance_->ResetToSystemDefaultTheme();
    QGuiApplication::styleHints()->setColorScheme(ui_->checkbox_dark_mode->isChecked() ? Qt::ColorScheme::Dark : Qt::ColorScheme::Unknown);
  }
  else {
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
    if (ui_->use_custom_color_set->isChecked()) {
      ApplyCustomColors();
    }
    else {
      appearance_->ResetToSystemDefaultTheme();
    }
  }
#endif

  set_changed();

}

void AppearanceSettingsPage::DarkModeToggled(const bool checked) {

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  QGuiApplication::styleHints()->setColorScheme(checked ? Qt::ColorScheme::Dark : Qt::ColorScheme::Unknown);
#else
  Q_UNUSED(checked)
#endif

  set_changed();

}

QString AppearanceSettingsPage::ColorRoleLabel(const QPalette::ColorRole color_role) {

  switch (color_role) {
    case QPalette::Window:          return tr("Window");
    case QPalette::WindowText:      return tr("Window text");
    case QPalette::Base:            return tr("Base");
    case QPalette::AlternateBase:   return tr("Alternate base");
    case QPalette::ToolTipBase:     return tr("Tooltip base");
    case QPalette::ToolTipText:     return tr("Tooltip text");
    case QPalette::PlaceholderText: return tr("Placeholder text");
    case QPalette::Text:            return tr("Text");
    case QPalette::Button:          return tr("Button");
    case QPalette::ButtonText:      return tr("Button text");
    case QPalette::BrightText:      return tr("Bright text");
    default:                        return QString();
  }

}

void AppearanceSettingsPage::CreateColorSelectors() {

  QFormLayout *layout = ui_->layout_custom_colors;

  for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
    QPushButton *button = new QPushButton(ui_->widget_custom_colors);
    button->setToolTip(tr("Select color"));
    const QPalette::ColorRole color_role_role = color_role.role;
    QObject::connect(button, &QPushButton::pressed, this, [this, color_role_role]() { SelectColor(color_role_role); });
    layout->addRow(ColorRoleLabel(color_role_role) + u':', button);
    color_selectors_.insert(color_role_role, button);
  }

}

void AppearanceSettingsPage::SelectColor(const QPalette::ColorRole role) {

  const QColor color_selected = QColorDialog::getColor(current_colors_.value(role));
  if (!color_selected.isValid()) return;

  current_colors_[role] = color_selected;

  if (color_selectors_.contains(role)) {
    UpdateColorSelectorColor(color_selectors_.value(role), color_selected);
  }

  ApplyCustomColors();

  set_changed();

}

void AppearanceSettingsPage::ApplyCustomColors() {
  Appearance::ChangeColors(current_colors_);
}

void AppearanceSettingsPage::SetDarkColors() {

  // Switch to a custom color set and fill it with colors suitable for a dark theme.
  ui_->use_custom_color_set->setChecked(true);

  const QMap<QPalette::ColorRole, QColor> dark_colors = Appearance::DarkColors();
  for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
    const QPalette::ColorRole color_role_role = color_role.role;
    if (dark_colors.contains(color_role_role)) {
      current_colors_[color_role_role] = dark_colors.value(color_role_role);
    }
  }

  UpdateColorSelectorsColors();
  ApplyCustomColors();

  set_changed();

}

void AppearanceSettingsPage::UseCustomColorSetOptionChanged(bool checked) {

  if (checked) {
    ApplyCustomColors();
  }
  else {
    // Only restore the system palette for the preview; keep the user's custom picks in current_colors_ so they are not lost when switching back to a custom color set.
    appearance_->ResetToSystemDefaultTheme();
  }

}

void AppearanceSettingsPage::ResetToDefaultColors() {

  // Reset the custom color set back to the system default colors.
  const QPalette p = appearance_->system_palette();
  for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
    current_colors_[color_role.role] = p.color(color_role.role);
  }

  UpdateColorSelectorsColors();

  if (ui_->use_custom_color_set->isChecked()) {
    ApplyCustomColors();
  }

  set_changed();

}

void AppearanceSettingsPage::UpdateColorSelectorsColors() {

  for (QMap<QPalette::ColorRole, QPushButton*>::const_iterator it = color_selectors_.constBegin(); it != color_selectors_.constEnd(); ++it) {
    UpdateColorSelectorColor(it.value(), current_colors_.value(it.key()));
  }

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

void AppearanceSettingsPage::BlurLevelChanged(const int value) {
  ui_->background_blur_radius_label->setText(QStringLiteral("%1px").arg(value));
}

void AppearanceSettingsPage::OpacityLevelChanged(const int percent) {
  ui_->background_opacity_label->setText(QStringLiteral("%1%").arg(percent));
}

void AppearanceSettingsPage::TabBarSystemColor(const bool checked) {

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

void AppearanceSettingsPage::PlaylistPlayingSongColorSystem(const bool checked) {

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

bool AppearanceSettingsPage::IsNativeStyle(const QString &style_name) {

#if defined(Q_OS_WIN32)
  return style_name.compare(u"default"_s, Qt::CaseInsensitive) == 0 || style_name.compare(u"windowsvista"_s, Qt::CaseInsensitive) == 0 || style_name.compare(u"windows11"_s, Qt::CaseInsensitive) == 0;
#elif defined(Q_OS_MACOS)
  return style_name.compare(u"default"_s, Qt::CaseInsensitive) == 0 || style_name.compare(u"macos"_s, Qt::CaseInsensitive) == 0;
#else
  Q_UNUSED(style_name)
  return false;
#endif

}

bool AppearanceSettingsPage::IsBreezeStyle() {

  // Breeze colors the menubar and toolbars ("Tools Area") from the Header color set of the KDE color scheme and applies it directly to the widgets, so a custom application palette is never fully in effect with Breeze.
  // This checks the active style, not the selected style, because style changes take effect on restart while palette colors apply to the running application.
  return QApplication::style() && QApplication::style()->objectName().startsWith(u"breeze"_s, Qt::CaseInsensitive);

}
