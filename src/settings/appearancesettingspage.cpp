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
#include <QPointer>
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
#include "utilities/styleutils.h"
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
      original_style_(QApplication::style() ? QApplication::style()->objectName() : QString()),
      system_palette_(QApplication::palette()),
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

  CreateColorSelectors();

  ui_->combobox_background_image_position->setItemData(0, static_cast<int>(BackgroundImagePosition::UpperLeft));
  ui_->combobox_background_image_position->setItemData(1, static_cast<int>(BackgroundImagePosition::UpperRight));
  ui_->combobox_background_image_position->setItemData(2, static_cast<int>(BackgroundImagePosition::Middle));
  ui_->combobox_background_image_position->setItemData(3, static_cast<int>(BackgroundImagePosition::BottomLeft));
  ui_->combobox_background_image_position->setItemData(4, static_cast<int>(BackgroundImagePosition::BottomRight));

  QObject::connect(ui_->checkbox_dark_mode, &QCheckBox::toggled, this, &AppearanceSettingsPage::DarkModeToggled);

  QObject::connect(ui_->use_custom_color_set, &QRadioButton::toggled, this, &AppearanceSettingsPage::UseCustomColorSetOptionChanged);
  QObject::connect(ui_->use_custom_color_set, &QRadioButton::toggled, ui_->widget_custom_colors, &QWidget::setEnabled);
  QObject::connect(ui_->button_dark_colors, &QPushButton::pressed, this, &AppearanceSettingsPage::SetDarkColors);
  QObject::connect(ui_->button_reset_colors, &QPushButton::pressed, this, &AppearanceSettingsPage::ResetToDefaultColors);

  QObject::connect(ui_->select_tabbar_color, &QPushButton::pressed, this, &AppearanceSettingsPage::TabBarSelectBGColor);
  QObject::connect(ui_->tabbar_system_color, &QRadioButton::toggled, this, &AppearanceSettingsPage::TabBarSystemColor);

  QObject::connect(ui_->select_playlist_playing_song_color, &QPushButton::pressed, this, &AppearanceSettingsPage::PlaylistPlayingSongSelectColor);
  QObject::connect(ui_->playlist_playing_song_color_system, &QRadioButton::toggled, this, &AppearanceSettingsPage::PlaylistPlayingSongColorSystem);

  QObject::connect(ui_->use_default_background, &QRadioButton::toggled, ui_->widget_background_image_options, &AppearanceSettingsPage::setDisabled);
  QObject::connect(ui_->use_no_background, &QRadioButton::toggled, ui_->widget_background_image_options, &AppearanceSettingsPage::setDisabled);
  QObject::connect(ui_->use_album_cover_background, &QRadioButton::toggled, ui_->widget_background_image_options, &AppearanceSettingsPage::setEnabled);
  QObject::connect(ui_->use_strawbs_background, &QRadioButton::toggled, ui_->widget_background_image_options, &AppearanceSettingsPage::setDisabled);
  QObject::connect(ui_->use_custom_background_image, &QRadioButton::toggled, ui_->widget_background_image_options, &AppearanceSettingsPage::setEnabled);

  QObject::connect(ui_->select_background_image_filename_button, &QPushButton::pressed, this, &AppearanceSettingsPage::SelectBackgroundImage);
  QObject::connect(ui_->use_custom_background_image, &QRadioButton::toggled, ui_->background_image_filename, &AppearanceSettingsPage::setEnabled);
  QObject::connect(ui_->use_custom_background_image, &QRadioButton::toggled, ui_->select_background_image_filename_button, &AppearanceSettingsPage::setEnabled);

  QObject::connect(ui_->checkbox_background_image_stretch, &QCheckBox::toggled, ui_->checkbox_background_image_do_not_cut, &AppearanceSettingsPage::setEnabled);
  QObject::connect(ui_->checkbox_background_image_stretch, &QCheckBox::toggled, ui_->checkbox_background_image_keep_aspect_ratio, &AppearanceSettingsPage::setEnabled);
  QObject::connect(ui_->checkbox_background_image_stretch, &QCheckBox::toggled, ui_->spinbox_background_image_maxsize, &AppearanceSettingsPage::setDisabled);

  QObject::connect(ui_->checkbox_background_image_keep_aspect_ratio, &QCheckBox::toggled, ui_->checkbox_background_image_do_not_cut, &AppearanceSettingsPage::setEnabled);

  QObject::connect(ui_->slider_background_image_blur, &QSlider::valueChanged, this, &AppearanceSettingsPage::BackgroundImageBlurLevelChanged);
  QObject::connect(ui_->slider_background_image_opacity, &QSlider::valueChanged, this, &AppearanceSettingsPage::BackgroundImageOpacityLevelChanged);

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  ui_->checkbox_system_icons->setEnabled(false);
#else
  ui_->checkbox_system_icons->setEnabled(true);
#endif

  AppearanceSettingsPage::Load();

}

AppearanceSettingsPage::~AppearanceSettingsPage() = default;

void AppearanceSettingsPage::Load() {

  // Disconnect while loading so intermediate combobox signals don't trigger StyleChanged before colors are ready.
  QObject::disconnect(ui_->combobox_style, &QComboBox::currentIndexChanged, this, &AppearanceSettingsPage::StyleChanged);

  Settings s;
  s.beginGroup(kSettingsGroup);

  // Style
  original_style_ = QApplication::style() ? QApplication::style()->objectName() : QString();
  ComboBoxLoadFromSettings(s, ui_->combobox_style, QLatin1String(kStyle), u"default"_s);

#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN32)
  ui_->checkbox_system_icons->setChecked(s.value(kSystemThemeIcons, kDefaultSystemIcons).toBool());
#endif

  original_dark_mode_ = s.value(kDarkMode, kDefaultDarkMode).toBool();
  ui_->checkbox_dark_mode->setChecked(original_dark_mode_);

  // Colors
  const QPalette palette = QApplication::palette();

  // Keep in mind originals colors, in case the user clicks on Cancel, to be able to restore colors
  original_use_custom_color_set_ = s.value(kUseCustomColorSet, kDefaultUseCustomColorSet).toBool();

  current_colors_.clear();
  for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
    const QVariant value = s.value(color_role.settings_key);
    QColor color = value.isValid() ? value.value<QColor>() : QColor();
    if (!color.isValid()) {
      color = palette.color(color_role.role);
    }
    current_colors_.insert(color_role.role, color);
  }

  original_colors_ = current_colors_;

  UpdateColorSelectorsColors();

  // Tabbar background color
  bool tabbar_system_color = s.value(kTabBarSystemColor, kDefaultTabBarSystemColor).toBool();
  ui_->tabbar_gradient->setChecked(s.value(kTabBarGradient, kDefaultTabBarGradient).toBool());
  ui_->tabbar_system_color->setChecked(tabbar_system_color);
  ui_->tabbar_custom_color->setChecked(!tabbar_system_color);

  current_tabbar_bg_color_ = s.value(kTabBarColor, FancyTabWidget::DefaultTabbarBgColor()).value<QColor>();

  UpdateColorSelectorColor(ui_->select_tabbar_color, current_tabbar_bg_color_);
  TabBarSystemColor(ui_->tabbar_system_color->isChecked());

  // Currently playing song color
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

  // Playlist background image
  {
    const int v = s.value(kBackgroundImageType, static_cast<int>(kDefaultBackgroundImageType)).toInt();
    background_image_type_ = (v >= static_cast<int>(BackgroundImageType::Default) && v <= static_cast<int>(BackgroundImageType::Strawbs)) ? static_cast<BackgroundImageType>(v) : kDefaultBackgroundImageType;
  }
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
    case BackgroundImageType::Custom:
      ui_->use_custom_background_image->setChecked(true);
      break;
    case BackgroundImageType::Album:
      ui_->use_album_cover_background->setChecked(true);
      break;
    case BackgroundImageType::Strawbs:
      ui_->use_strawbs_background->setChecked(true);
      break;
  }
  ui_->background_image_filename->setText(background_image_filename_);

  const int background_image_position = s.value(kBackgroundImagePosition, static_cast<int>(kDefaultBackgroundImagePosition)).toInt();
  int background_image_position_index = ui_->combobox_background_image_position->findData(background_image_position);
  if (background_image_position_index < 0) {
    background_image_position_index = ui_->combobox_background_image_position->findData(static_cast<int>(kDefaultBackgroundImagePosition));
  }
  ui_->combobox_background_image_position->setCurrentIndex(background_image_position_index);

  ui_->spinbox_background_image_maxsize->setValue(s.value(kBackgroundImageMaxSize, kDefaultBackgroundImageMaxSize).toInt());
  ui_->checkbox_background_image_stretch->setChecked(s.value(kBackgroundImageStretch, kDefaultBackgroundImageStretch).toBool());
  ui_->checkbox_background_image_do_not_cut->setChecked(s.value(kBackgroundImageDoNotCut, kDefaultBackgroundImageDoNotCut).toBool());
  ui_->checkbox_background_image_keep_aspect_ratio->setChecked(s.value(kBackgroundImageKeepAspectRatio, kDefaultBackgroundImageKeepAspectRatio).toBool());
  ui_->slider_background_image_blur->setValue(s.value(kBackgroundImageBlurRadius, kDefaultBackgroundImageBlurRadius).toInt());
  ui_->slider_background_image_opacity->setValue(s.value(kBackgroundImageOpacityLevel, kDefaultBackgroundImageOpacityLevel).toInt());

  ui_->checkbox_background_image_keep_aspect_ratio->setEnabled(ui_->checkbox_background_image_stretch->isChecked());
  ui_->checkbox_background_image_do_not_cut->setEnabled(ui_->checkbox_background_image_stretch->isChecked() && ui_->checkbox_background_image_keep_aspect_ratio->isChecked());

  // Button sizes
  ui_->spinbox_icon_size_tabbar_small_mode->setValue(s.value(kIconSizeTabbarSmallMode, kDefaultIconSizeTabbarSmallMode).toInt());
  ui_->spinbox_icon_size_tabbar_large_mode->setValue(s.value(kIconSizeTabbarLargeMode, kDefaultIconSizeTabbarLargeMode).toInt());
  ui_->spinbox_icon_size_play_control_buttons->setValue(s.value(kIconSizePlayControlButtons, kDefaultIconSizePlayControlButtons).toInt());
  ui_->spinbox_icon_size_playlist_buttons->setValue(s.value(kIconSizePlaylistButtons, kDefaultIconSizePlaylistButtons).toInt());
  ui_->spinbox_icon_size_left_panel_buttons->setValue(s.value(kIconSizeLeftPanelButtons, kDefaultIconSizeLeftPanelButtons).toInt());
  ui_->spinbox_icon_size_configure_buttons->setValue(s.value(kIconSizeConfigureButtons, kDefaultIconSizeConfigureButtons).toInt());

  original_tabbar_bg_color_ = current_tabbar_bg_color_;
  original_background_image_filename_ = background_image_filename_;
  original_playlist_playing_song_color_ = current_playlist_playing_song_color_;

  s.endGroup();

  Init(ui_->layout_appearancesettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

  QObject::connect(ui_->combobox_style, &QComboBox::currentIndexChanged, this, &AppearanceSettingsPage::StyleChanged);
  InitStyle(ui_->combobox_style->currentIndex());

}

void AppearanceSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  // Style
  s.setValue(kStyle, ui_->combobox_style->currentText());

  QString selected_style = ui_->combobox_style->currentData().toString();
  if (selected_style.compare(u"default"_s, Qt::CaseInsensitive) == 0) {
    selected_style = appearance_->default_style();
  }

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  s.setValue(kSystemThemeIcons, false);
#else
  s.setValue(kSystemThemeIcons, ui_->checkbox_system_icons->isChecked());
#endif

  s.setValue(kDarkMode, Utilities::StyleHasDarkModeSupport(selected_style) ? ui_->checkbox_dark_mode->isChecked() : false);

  // Colors
  const bool use_custom_color_set = Utilities::StyleHasCustomPaletteColorsSupport(selected_style) && ui_->use_custom_color_set->isChecked();
  s.setValue(kUseCustomColorSet, use_custom_color_set);
  if (use_custom_color_set) {
    for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
      s.setValue(color_role.settings_key, current_colors_.value(color_role.role));
    }
  }
  else {
    QApplication::setPalette(system_palette_);
    for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
      s.remove(color_role.settings_key);
    }
  }

  // Tabbar background color
  s.setValue(kTabBarSystemColor, ui_->tabbar_system_color->isChecked());
  s.setValue(kTabBarGradient, ui_->tabbar_gradient->isChecked());
  s.setValue(kTabBarColor, current_tabbar_bg_color_);

  // Currently playing song color
  if (ui_->playlist_playing_song_color_system->isChecked()) {
    s.setValue(kPlaylistPlayingSongColor, QColor());
  }
  else {
    s.setValue(kPlaylistPlayingSongColor, current_playlist_playing_song_color_);
  }

  // Playlist background image
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
  s.setValue(kBackgroundImagePosition, ui_->combobox_background_image_position->currentData().toInt());
  s.setValue(kBackgroundImageStretch, ui_->checkbox_background_image_stretch->isChecked());
  s.setValue(kBackgroundImageDoNotCut, ui_->checkbox_background_image_do_not_cut->isChecked());
  s.setValue(kBackgroundImageKeepAspectRatio, ui_->checkbox_background_image_keep_aspect_ratio->isChecked());

  s.setValue(kBackgroundImageBlurRadius, ui_->slider_background_image_blur->value());
  s.setValue(kBackgroundImageOpacityLevel, ui_->slider_background_image_opacity->value());

  // Button sizes
  s.setValue(kIconSizeTabbarSmallMode, ui_->spinbox_icon_size_tabbar_small_mode->value());
  s.setValue(kIconSizeTabbarLargeMode, ui_->spinbox_icon_size_tabbar_large_mode->value());
  s.setValue(kIconSizePlayControlButtons, ui_->spinbox_icon_size_play_control_buttons->value());
  s.setValue(kIconSizePlaylistButtons, ui_->spinbox_icon_size_playlist_buttons->value());
  s.setValue(kIconSizeLeftPanelButtons, ui_->spinbox_icon_size_left_panel_buttons->value());
  s.setValue(kIconSizeConfigureButtons, ui_->spinbox_icon_size_configure_buttons->value());

  s.endGroup();

  appearance_->set_system_palette(system_palette_);

}

void AppearanceSettingsPage::Cancel() {

  const QString current_style = QApplication::style() ? QApplication::style()->objectName() : QString();
  if (original_style_.compare(current_style, Qt::CaseInsensitive) != 0) {
    ApplyStyle(current_style, original_style_);
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  QGuiApplication::styleHints()->setColorScheme(original_dark_mode_ ? Qt::ColorScheme::Dark : Qt::ColorScheme::Unknown);
#endif

  if (original_use_custom_color_set_) {
    Appearance::SetCustomPaletteColors(original_colors_);
  }
  else {
    QApplication::setPalette(system_palette_);
  }

  background_image_filename_ = original_background_image_filename_;
  current_tabbar_bg_color_ = original_tabbar_bg_color_;
  current_playlist_playing_song_color_ = original_playlist_playing_song_color_;

}

void AppearanceSettingsPage::InitStyle(const int index) {

  SetStyle(index, false);

}

void AppearanceSettingsPage::StyleChanged(const int index) {

  SetStyle(index, true);

}

void AppearanceSettingsPage::SetStyle(const int index, const bool apply) {

  const QString style_name = ui_->combobox_style->itemData(index).toString();
  SetStyle(style_name, apply);

}

void AppearanceSettingsPage::SetStyle(const QString &new_style, const bool apply) {

  QString current_style = QApplication::style() ? QApplication::style()->objectName() : QString();

  bool style_changed = false;
  if (apply) {
    style_changed = ApplyStyle(current_style, new_style);
    current_style = QApplication::style() ? QApplication::style()->objectName() : QString();
  }

  const QString style_for_caps = apply ? current_style : (new_style.compare("default"_L1, Qt::CaseInsensitive) == 0 ? appearance_->default_style() : new_style);
  const bool custom_palette_colors_support = Utilities::StyleHasCustomPaletteColorsSupport(style_for_caps);
  const bool dark_mode_support = Utilities::StyleHasDarkModeSupport(style_for_caps);

  if (!dark_mode_support && ui_->checkbox_dark_mode->isChecked()) {
    ui_->checkbox_dark_mode->setChecked(false);
  }

  if (!custom_palette_colors_support && !ui_->use_system_color_set->isChecked()) {
    ui_->use_system_color_set->setChecked(true);
  }

  ui_->checkbox_dark_mode->setEnabled(dark_mode_support);
  ui_->groupbox_colors->setEnabled(custom_palette_colors_support);

  if (style_changed) {
    QApplication::setPalette(QPalette());
    system_palette_ = QApplication::palette();
    if (ui_->use_custom_color_set->isChecked()) {
      ApplyCustomColors();
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(dark_mode_support && ui_->checkbox_dark_mode->isChecked() ? Qt::ColorScheme::Dark : Qt::ColorScheme::Unknown);
#endif
  }

}

bool AppearanceSettingsPage::ApplyStyle(const QString &current_style, const QString &new_style) {

  QString effective_new_style = new_style;
  if (new_style.compare("default"_L1, Qt::CaseInsensitive) == 0) {
    effective_new_style = appearance_->default_style();
  }
  if (current_style.compare(effective_new_style, Qt::CaseInsensitive) != 0) {
    qLog(Debug) << "Changing style from" << current_style << "to" << effective_new_style;
    if (QApplication::setStyle(effective_new_style)) {
      qLog(Debug) << "Style is set to" << effective_new_style;
      return true;
    }
    else {
      qLog(Warning) << "Could not set style" << effective_new_style;
    }
  }

  return false;

}

void AppearanceSettingsPage::DarkModeToggled(const bool checked) {

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  QGuiApplication::styleHints()->setColorScheme(checked ? Qt::ColorScheme::Dark : Qt::ColorScheme::Unknown);
#else
  Q_UNUSED(checked)
#endif

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
    default: return QString();
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

  const auto it = color_selectors_.constFind(role);
  if (it != color_selectors_.constEnd() && it.value()) {
    UpdateColorSelectorColor(it.value(), color_selected);
  }

  ApplyCustomColors();

  set_changed();

}

void AppearanceSettingsPage::ApplyCustomColors() {
  Appearance::SetCustomPaletteColors(current_colors_);
}

void AppearanceSettingsPage::SetDarkColors() {

  // Switch to a custom color set and fill it with colors suitable for a dark theme.
  ui_->use_custom_color_set->setChecked(true);

  const QMap<QPalette::ColorRole, QColor> &dark_colors = Appearance::DarkColors();
  for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
    current_colors_[color_role.role] = dark_colors.value(color_role.role);
  }

  UpdateColorSelectorsColors();
  ApplyCustomColors();

  set_changed();

}

void AppearanceSettingsPage::UseCustomColorSetOptionChanged(const bool checked) {

  if (checked) {
    ApplyCustomColors();
  }
  else {
    QApplication::setPalette(system_palette_);
  }

}

void AppearanceSettingsPage::ResetToDefaultColors() {

  // Reset the custom color set back to the system default colors.
  const QPalette &p = system_palette_;
  for (const Appearance::ColorRole &color_role : Appearance::ColorRoles()) {
    current_colors_[color_role.role] = p.color(color_role.role);
  }

  UpdateColorSelectorsColors();

  if (ui_->use_custom_color_set->isChecked()) {
    ApplyCustomColors();
  }

  set_changed();

}

void AppearanceSettingsPage::UpdateColorSelectorsColors() const {

  for (auto it = color_selectors_.constBegin(); it != color_selectors_.constEnd(); ++it) {
    if (it.value()) {
      UpdateColorSelectorColor(it.value(), current_colors_.value(it.key()));
    }
  }

}

void AppearanceSettingsPage::UpdateColorSelectorColor(QWidget *color_selector, const QColor &color) {

  const QString css = QStringLiteral("background-color: rgb(%1, %2, %3); color: rgb(255, 255, 255); border: 1px dotted black;").arg(color.red()).arg(color.green()).arg(color.blue());
  if (color_selector->styleSheet() != css) {
    color_selector->setStyleSheet(css);
  }

}

void AppearanceSettingsPage::SelectBackgroundImage() {

  QString selected_filename = QFileDialog::getOpenFileName(this, tr("Select background image"), background_image_filename_, tr(kLoadImageFileFilter) + u";;"_s + tr(kAllFilesFilterSpec));
  if (selected_filename.isEmpty()) return;
  background_image_filename_ = selected_filename;
  ui_->background_image_filename->setText(background_image_filename_);

}

void AppearanceSettingsPage::BackgroundImageBlurLevelChanged(const int value) {
  ui_->label_background_image_blur_radius->setText(QStringLiteral("%1px").arg(value));
}

void AppearanceSettingsPage::BackgroundImageOpacityLevelChanged(const int percent) {
  ui_->label_background_image_opacity->setText(QStringLiteral("%1%").arg(percent));
}

void AppearanceSettingsPage::TabBarSystemColor(const bool checked) {

  if (checked) {
    current_tabbar_bg_color_ = FancyTabWidget::DefaultTabbarBgColor();
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

  set_changed();

}

void AppearanceSettingsPage::PlaylistPlayingSongColorSystem(const bool checked) {

  if (checked) {
    current_playlist_playing_song_color_ = StyleHelper::highlightColor();
    UpdateColorSelectorColor(ui_->select_playlist_playing_song_color, current_playlist_playing_song_color_);
  }
  ui_->layout_playlist_playing_song_color_custom->setEnabled(!checked);
  ui_->select_playlist_playing_song_color->setEnabled(!checked);

}

void AppearanceSettingsPage::PlaylistPlayingSongSelectColor() {

  if (ui_->playlist_playing_song_color_system->isChecked()) return;

  QColor color_selected = QColorDialog::getColor(current_playlist_playing_song_color_);
  if (!color_selected.isValid()) return;
  current_playlist_playing_song_color_ = color_selected;
  UpdateColorSelectorColor(ui_->select_playlist_playing_song_color, current_playlist_playing_song_color_);

  set_changed();

}
