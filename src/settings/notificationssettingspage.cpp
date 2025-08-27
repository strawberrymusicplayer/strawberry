/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QAction>
#include <QImage>
#include <QColor>
#include <QRgb>
#include <QMenu>
#include <QFont>
#include <QFontDialog>
#include <QCursor>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QToolTip>
#include <QtEvents>
#include <QSettings>

#include "core/iconloader.h"
#include "core/settings.h"
#include "osd/osdbase.h"
#include "osd/osdpretty.h"
#include "settingspage.h"
#include "settingsdialog.h"
#include "notificationssettingspage.h"
#include "ui_notificationssettingspage.h"
#include "constants/notificationssettings.h"

using namespace Qt::Literals::StringLiterals;

class QHideEvent;
class QShowEvent;

NotificationsSettingsPage::NotificationsSettingsPage(SettingsDialog *dialog, OSDBase *osd, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_NotificationsSettingsPage),
      osd_(osd),
      pretty_popup_(new OSDPretty(OSDPretty::Mode::Draggable)) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"help-hint"_s, true, 0, 32));

  pretty_popup_->SetMessage(tr("OSD Preview"), tr("Drag to reposition"), QImage(u":/pictures/cdcase.png"_s));

  ui_->notifications_bg_preset->setItemData(0, QColor(OSDPrettySettings::kPresetBlue), Qt::DecorationRole);
  ui_->notifications_bg_preset->setItemData(1, QColor(OSDPrettySettings::kPresetRed), Qt::DecorationRole);

  // Create and populate the helper menus
  QMenu *menu = new QMenu(this);
  menu->addAction(ui_->action_title);
  menu->addAction(ui_->action_album);
  menu->addAction(ui_->action_artist);
  menu->addAction(ui_->action_albumartist);
  menu->addAction(ui_->action_disc);
  menu->addAction(ui_->action_track);
  menu->addAction(ui_->action_year);
  menu->addAction(ui_->action_originalyear);
  menu->addAction(ui_->action_genre);
  menu->addAction(ui_->action_composer);
  menu->addAction(ui_->action_performer);
  menu->addAction(ui_->action_grouping);
  menu->addAction(ui_->action_length);
  menu->addAction(ui_->action_filename);
  menu->addAction(ui_->action_url);
  menu->addAction(ui_->action_playcount);
  menu->addAction(ui_->action_skipcount);
  menu->addAction(ui_->action_rating);
  menu->addSeparator();
  menu->addAction(ui_->action_newline);
  ui_->notifications_exp_chooser1->setMenu(menu);
  ui_->notifications_exp_chooser2->setMenu(menu);
  ui_->notifications_exp_chooser1->setPopupMode(QToolButton::InstantPopup);
  ui_->notifications_exp_chooser2->setPopupMode(QToolButton::InstantPopup);
  // We need this because by default menus don't show tooltips
  QObject::connect(menu, &QMenu::hovered, this, &NotificationsSettingsPage::ShowMenuTooltip);

  QObject::connect(ui_->notifications_none, &QRadioButton::toggled, this, &NotificationsSettingsPage::NotificationTypeChanged);
  QObject::connect(ui_->notifications_native, &QRadioButton::toggled, this, &NotificationsSettingsPage::NotificationTypeChanged);
  QObject::connect(ui_->notifications_tray, &QRadioButton::toggled, this, &NotificationsSettingsPage::NotificationTypeChanged);
  QObject::connect(ui_->notifications_pretty, &QRadioButton::toggled, this, &NotificationsSettingsPage::NotificationTypeChanged);
  QObject::connect(ui_->notifications_opacity, &QSlider::valueChanged, this, &NotificationsSettingsPage::PrettyOpacityChanged);
  QObject::connect(ui_->notifications_bg_preset, QOverload<int>::of(&QComboBox::activated), this, &NotificationsSettingsPage::PrettyColorPresetChanged);
  QObject::connect(ui_->notifications_fg_choose, &QPushButton::clicked, this, &NotificationsSettingsPage::ChooseFgColor);
  QObject::connect(ui_->notifications_font_choose, &QPushButton::clicked, this, &NotificationsSettingsPage::ChooseFont);
  QObject::connect(ui_->notifications_exp_chooser1, &QToolButton::triggered, this, &NotificationsSettingsPage::InsertVariableFirstLine);
  QObject::connect(ui_->notifications_exp_chooser2, &QToolButton::triggered, this, &NotificationsSettingsPage::InsertVariableSecondLine);
  QObject::connect(ui_->notifications_disable_duration, &QCheckBox::toggled, ui_->notifications_duration, &NotificationsSettingsPage::setDisabled);

  ui_->notifications_native->setEnabled(osd_->SupportsNativeNotifications());
  ui_->notifications_tray->setEnabled(osd_->SupportsTrayPopups());
  ui_->notifications_pretty->setEnabled(osd_->SupportsOSDPretty());

  QObject::connect(ui_->notifications_pretty, &QRadioButton::toggled, this, &NotificationsSettingsPage::UpdatePopupVisible);

  QObject::connect(ui_->notifications_custom_text_enabled, &QCheckBox::toggled, this, &NotificationsSettingsPage::NotificationCustomTextChanged);
  QObject::connect(ui_->notifications_preview, &QPushButton::clicked, this, &NotificationsSettingsPage::PrepareNotificationPreview);

  // Icons
  ui_->notifications_exp_chooser1->setIcon(IconLoader::Load(u"list-add"_s));
  ui_->notifications_exp_chooser2->setIcon(IconLoader::Load(u"list-add"_s));

  QObject::connect(pretty_popup_, &OSDPretty::PositionChanged, this, &NotificationsSettingsPage::PrettyOSDChanged);
  QObject::connect(ui_->richpresence_enabled, &QCheckBox::toggled, this, &NotificationsSettingsPage::DiscordRPCChanged);

}

NotificationsSettingsPage::~NotificationsSettingsPage() {
  delete pretty_popup_;
  delete ui_;
}

void NotificationsSettingsPage::showEvent(QShowEvent *e) {
  Q_UNUSED(e)
  UpdatePopupVisible();
}

void NotificationsSettingsPage::hideEvent(QHideEvent *e) {
  Q_UNUSED(e)
  UpdatePopupVisible();
}

void NotificationsSettingsPage::Load() {

  Settings s;

  s.beginGroup(OSDSettings::kSettingsGroup);
  OSDSettings::Type osd_type = static_cast<OSDSettings::Type>(s.value(OSDSettings::kType, static_cast<int>(OSDSettings::Type::Native)).toInt());
  if (!osd_->IsTypeSupported(osd_type)) {
    osd_type = osd_->GetSupportedType();
  }
  switch (osd_type) {
    case OSDSettings::Type::Native:
      ui_->notifications_native->setChecked(true);
      break;
    case OSDSettings::Type::Pretty:
      ui_->notifications_pretty->setChecked(true);
      break;
    case OSDSettings::Type::TrayPopup:
      ui_->notifications_tray->setChecked(true);
      break;
    case OSDSettings::Type::Disabled:
    default:
      ui_->notifications_none->setChecked(true);
      break;
  }
  ui_->notifications_duration->setValue(s.value(OSDSettings::kTimeout, 5000).toInt() / 1000);
  ui_->notifications_volume->setChecked(s.value(OSDSettings::kShowOnVolumeChange, false).toBool());
  ui_->notifications_play_mode->setChecked(s.value(OSDSettings::kShowOnPlayModeChange, true).toBool());
  ui_->notifications_pause->setChecked(s.value(OSDSettings::kShowOnPausePlayback, true).toBool());
  ui_->notifications_resume->setChecked(s.value(OSDSettings::kShowOnResumePlayback, false).toBool());
  ui_->notifications_art->setChecked(s.value(OSDSettings::kShowArt, true).toBool());
  ui_->notifications_custom_text_enabled->setChecked(s.value(OSDSettings::kCustomTextEnabled, false).toBool());
  ui_->notifications_custom_text1->setText(s.value(OSDSettings::kCustomText1).toString());
  ui_->notifications_custom_text2->setText(s.value(OSDSettings::kCustomText2).toString());
  s.endGroup();

#ifdef Q_OS_MACOS
  ui_->notifications_options->setEnabled(ui_->notifications_pretty->isChecked());
#endif

  // Pretty OSD
  pretty_popup_->ReloadSettings();
  ui_->notifications_opacity->setValue(static_cast<int>(pretty_popup_->background_opacity() * 100));

  QRgb color = pretty_popup_->background_color();
  if (color == OSDPrettySettings::kPresetBlue) {
    ui_->notifications_bg_preset->setCurrentIndex(0);
  }
  else if (color == OSDPrettySettings::kPresetRed) {
    ui_->notifications_bg_preset->setCurrentIndex(1);
  }
  else {
    ui_->notifications_bg_preset->setCurrentIndex(2);
  }
  ui_->notifications_bg_preset->setItemData(2, QColor(color), Qt::DecorationRole);
  ui_->notifications_disable_duration->setChecked(pretty_popup_->disable_duration());

  ui_->notifications_fading->setChecked(pretty_popup_->fading());

  // Discord
  s.beginGroup(DiscordRPCSettings::kSettingsGroup);
  ui_->richpresence_enabled->setChecked(s.value(DiscordRPCSettings::kEnabled, false).toBool());

  const DiscordRPCSettings::StatusDisplayType discord_status_display_type = static_cast<DiscordRPCSettings::StatusDisplayType>(s.value(DiscordRPCSettings::kStatusDisplayType, static_cast<int>(DiscordRPCSettings::StatusDisplayType::App)).toInt());
  switch (discord_status_display_type) {
    case DiscordRPCSettings::StatusDisplayType::App:
      ui_->richpresence_listening_to_app->setChecked(true);
      break;
    case DiscordRPCSettings::StatusDisplayType::Artist:
      ui_->richpresence_listening_to_artist->setChecked(true);
      break;
    case DiscordRPCSettings::StatusDisplayType::Song:
      ui_->richpresence_listening_to_song->setChecked(true);
      break;
  }
  s.endGroup();

  UpdatePopupVisible();

  Init(ui_->layout_notificationssettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(OSDSettings::kSettingsGroup))) set_changed();

}

void NotificationsSettingsPage::Save() {

  Settings s;

  OSDSettings::Type osd_type = OSDSettings::Type::Disabled;
  if      (ui_->notifications_none->isChecked())   osd_type = OSDSettings::Type::Disabled;
  else if (osd_->SupportsNativeNotifications() && ui_->notifications_native->isChecked()) osd_type = OSDSettings::Type::Native;
  else if (osd_->SupportsTrayPopups() && ui_->notifications_tray->isChecked()) osd_type = OSDSettings::Type::TrayPopup;
  else if (osd_->SupportsOSDPretty() && ui_->notifications_pretty->isChecked()) osd_type = OSDSettings::Type::Pretty;

  DiscordRPCSettings::StatusDisplayType discord_status_display_type = DiscordRPCSettings::StatusDisplayType::App;
  if      (ui_->richpresence_listening_to_app->isChecked()) discord_status_display_type = DiscordRPCSettings::StatusDisplayType::App;
  else if (ui_->richpresence_listening_to_artist->isChecked()) discord_status_display_type = DiscordRPCSettings::StatusDisplayType::Artist;
  else if (ui_->richpresence_listening_to_song->isChecked()) discord_status_display_type = DiscordRPCSettings::StatusDisplayType::Song;

  s.beginGroup(OSDSettings::kSettingsGroup);
  s.setValue(OSDSettings::kType, static_cast<int>(osd_type));
  s.setValue(OSDSettings::kTimeout, ui_->notifications_duration->value() * 1000);
  s.setValue(OSDSettings::kShowOnVolumeChange, ui_->notifications_volume->isChecked());
  s.setValue(OSDSettings::kShowOnPlayModeChange, ui_->notifications_play_mode->isChecked());
  s.setValue(OSDSettings::kShowOnPausePlayback, ui_->notifications_pause->isChecked());
  s.setValue(OSDSettings::kShowOnResumePlayback, ui_->notifications_resume->isChecked());
  s.setValue(OSDSettings::kShowArt, ui_->notifications_art->isChecked());
  s.setValue(OSDSettings::kCustomTextEnabled, ui_->notifications_custom_text_enabled->isChecked());
  s.setValue(OSDSettings::kCustomText1, ui_->notifications_custom_text1->text());
  s.setValue(OSDSettings::kCustomText2, ui_->notifications_custom_text2->text());
  s.endGroup();

  s.beginGroup(OSDPrettySettings::kSettingsGroup);
  s.setValue(OSDPrettySettings::kForegroundColor, pretty_popup_->foreground_color());
  s.setValue(OSDPrettySettings::kBackgroundColor, pretty_popup_->background_color());
  s.setValue(OSDPrettySettings::kBackgroundOpacity, pretty_popup_->background_opacity());
  s.setValue(OSDPrettySettings::kPopupScreen, pretty_popup_->popup_screen());
  s.setValue(OSDPrettySettings::kPopupPos, pretty_popup_->popup_pos());
  s.setValue(OSDPrettySettings::kFont, pretty_popup_->font().toString());
  s.setValue(OSDPrettySettings::kDisableDuration, ui_->notifications_disable_duration->isChecked());
  s.setValue(OSDPrettySettings::kFading, ui_->notifications_fading->isChecked());
  s.endGroup();

  s.beginGroup(DiscordRPCSettings::kSettingsGroup);
  s.setValue(DiscordRPCSettings::kEnabled, ui_->richpresence_enabled->isChecked());
  s.setValue(DiscordRPCSettings::kStatusDisplayType, static_cast<int>(discord_status_display_type));
  s.endGroup();

}

void NotificationsSettingsPage::PrettyOpacityChanged(int value) {

  pretty_popup_->set_background_opacity(static_cast<qreal>(value) / 100.0);
  set_changed();

}

void NotificationsSettingsPage::UpdatePopupVisible() {

  pretty_popup_->setVisible(isVisible() && ui_->notifications_pretty->isChecked());

}

void NotificationsSettingsPage::PrettyColorPresetChanged(int index) {

  if (dialog()->is_loading_settings()) return;

  switch (index) {
    case 0:
      pretty_popup_->set_background_color(OSDPrettySettings::kPresetBlue);
      break;

    case 1:
      pretty_popup_->set_background_color(OSDPrettySettings::kPresetRed);
      break;

    case 2:
    default:
      ChooseBgColor();
      break;
  }

  set_changed();

}

void NotificationsSettingsPage::ChooseBgColor() {

  QColor color = QColorDialog::getColor(pretty_popup_->background_color(), this);
  if (!color.isValid()) return;

  pretty_popup_->set_background_color(color.rgb());
  ui_->notifications_bg_preset->setItemData(2, color, Qt::DecorationRole);

  set_changed();

}

void NotificationsSettingsPage::ChooseFgColor() {

  QColor color = QColorDialog::getColor(pretty_popup_->foreground_color(), this);
  if (!color.isValid()) return;

  pretty_popup_->set_foreground_color(color.rgb());

  set_changed();

}

void NotificationsSettingsPage::ChooseFont() {

  bool ok = false;
  QFont font = QFontDialog::getFont(&ok, pretty_popup_->font(), this);
  if (ok) {
    pretty_popup_->set_font(font);
    set_changed();
  }

}

void NotificationsSettingsPage::NotificationCustomTextChanged(bool enabled) {

  ui_->notifications_custom_text1->setEnabled(enabled);
  ui_->notifications_custom_text2->setEnabled(enabled);
  ui_->notifications_exp_chooser1->setEnabled(enabled);
  ui_->notifications_exp_chooser2->setEnabled(enabled);
  ui_->notifications_preview->setEnabled(enabled);
  ui_->label_summary->setEnabled(enabled);
  ui_->label_body->setEnabled(enabled);

}

void NotificationsSettingsPage::PrepareNotificationPreview() {

  OSDSettings::Type notificationType = OSDSettings::Type::Disabled;
  if (ui_->notifications_native->isChecked()) {
    notificationType = OSDSettings::Type::Native;
  }
  else if (ui_->notifications_pretty->isChecked()) {
    notificationType = OSDSettings::Type::Pretty;
  }
  else if (ui_->notifications_tray->isChecked()) {
    notificationType = OSDSettings::Type::TrayPopup;
  }

  // If user changes timeout or other options, that won't be reflected in the preview
  Q_EMIT NotificationPreview(notificationType, ui_->notifications_custom_text1->text(), ui_->notifications_custom_text2->text());

}

void NotificationsSettingsPage::InsertVariableFirstLine(QAction *action) {
  // We use action name, therefore those shouldn't be translatable
  ui_->notifications_custom_text1->insert(action->text());
}

void NotificationsSettingsPage::InsertVariableSecondLine(QAction *action) {
  // We use action name, therefore those shouldn't be translatable
  ui_->notifications_custom_text2->insert(action->text());
}

void NotificationsSettingsPage::ShowMenuTooltip(QAction *action) {
  QToolTip::showText(QCursor::pos(), action->toolTip());
}

void NotificationsSettingsPage::NotificationTypeChanged() {

  bool enabled = !ui_->notifications_none->isChecked();
  bool pretty = ui_->notifications_pretty->isChecked();
  bool tray = ui_->notifications_tray->isChecked();

  ui_->notifications_general->setEnabled(enabled);
  ui_->notifications_pretty_group->setEnabled(pretty);
  ui_->notifications_custom_text_group->setEnabled(enabled);
  ui_->notifications_art->setEnabled(!tray);

#ifdef Q_OS_MACOS
  ui_->notifications_options->setEnabled(pretty);
#endif
  ui_->notifications_duration->setEnabled(!pretty || !ui_->notifications_disable_duration->isChecked());
  ui_->notifications_disable_duration->setEnabled(pretty);

}

void NotificationsSettingsPage::PrettyOSDChanged() {
  set_changed();
}

void NotificationsSettingsPage::DiscordRPCChanged() {
  set_changed();
}
