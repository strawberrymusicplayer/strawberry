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

class QHideEvent;
class QShowEvent;

NotificationsSettingsPage::NotificationsSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_NotificationsSettingsPage),
      pretty_popup_(new OSDPretty(OSDPretty::Mode::Draggable)) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(QStringLiteral("help-hint"), true, 0, 32));

  pretty_popup_->SetMessage(tr("OSD Preview"), tr("Drag to reposition"), QImage(QStringLiteral(":/pictures/cdcase.png")));

  ui_->notifications_bg_preset->setItemData(0, QColor(OSDPretty::kPresetBlue), Qt::DecorationRole);
  ui_->notifications_bg_preset->setItemData(1, QColor(OSDPretty::kPresetRed), Qt::DecorationRole);

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

#ifdef Q_OS_WIN32
  if (!dialog->osd()->SupportsNativeNotifications() && !dialog->osd()->SupportsTrayPopups()) {
    ui_->notifications_native->setEnabled(false);
  }
#else
  if (!dialog->osd()->SupportsNativeNotifications()) {
    ui_->notifications_native->setEnabled(false);
  }
#endif
  if (!dialog->osd()->SupportsTrayPopups()) {
    ui_->notifications_tray->setEnabled(false);
  }

  QObject::connect(ui_->notifications_pretty, &QRadioButton::toggled, this, &NotificationsSettingsPage::UpdatePopupVisible);

  QObject::connect(ui_->notifications_custom_text_enabled, &QCheckBox::toggled, this, &NotificationsSettingsPage::NotificationCustomTextChanged);
  QObject::connect(ui_->notifications_preview, &QPushButton::clicked, this, &NotificationsSettingsPage::PrepareNotificationPreview);

  // Icons
  ui_->notifications_exp_chooser1->setIcon(IconLoader::Load(QStringLiteral("list-add")));
  ui_->notifications_exp_chooser2->setIcon(IconLoader::Load(QStringLiteral("list-add")));

  QObject::connect(pretty_popup_, &OSDPretty::PositionChanged, this, &NotificationsSettingsPage::PrettyOSDChanged);

}

NotificationsSettingsPage::~NotificationsSettingsPage() {
  delete pretty_popup_;
  delete ui_;
}

void NotificationsSettingsPage::showEvent(QShowEvent*) {
  UpdatePopupVisible();
}

void NotificationsSettingsPage::hideEvent(QHideEvent*) {
  UpdatePopupVisible();
}

void NotificationsSettingsPage::Load() {

  Settings s;

  s.beginGroup(OSDBase::kSettingsGroup);
  const OSDBase::Behaviour osd_behaviour = static_cast<OSDBase::Behaviour>(s.value("Behaviour", static_cast<int>(OSDBase::Behaviour::Native)).toInt());
  switch (osd_behaviour) {
    case OSDBase::Behaviour::Native:
#ifdef Q_OS_WIN32
      if (dialog()->osd()->SupportsNativeNotifications() || dialog()->osd()->SupportsTrayPopups()) {
#else
      if (dialog()->osd()->SupportsNativeNotifications()) {
#endif
        ui_->notifications_native->setChecked(true);
        break;
      }
      // Fallthrough

    case OSDBase::Behaviour::Pretty:
      ui_->notifications_pretty->setChecked(true);
      break;

    case OSDBase::Behaviour::TrayPopup:
      if (dialog()->osd()->SupportsTrayPopups()) {
        ui_->notifications_tray->setChecked(true);
        break;
      }
      // Fallthrough

    case OSDBase::Behaviour::Disabled:
    default:
      ui_->notifications_none->setChecked(true);
      break;
  }
  ui_->notifications_duration->setValue(s.value("Timeout", 5000).toInt() / 1000);
  ui_->notifications_volume->setChecked(s.value("ShowOnVolumeChange", false).toBool());
  ui_->notifications_play_mode->setChecked(s.value("ShowOnPlayModeChange", true).toBool());
  ui_->notifications_pause->setChecked(s.value("ShowOnPausePlayback", true).toBool());
  ui_->notifications_resume->setChecked(s.value("ShowOnResumePlayback", false).toBool());
  ui_->notifications_art->setChecked(s.value("ShowArt", true).toBool());
  ui_->notifications_custom_text_enabled->setChecked(s.value("CustomTextEnabled", false).toBool());
  ui_->notifications_custom_text1->setText(s.value("CustomText1").toString());
  ui_->notifications_custom_text2->setText(s.value("CustomText2").toString());
  s.endGroup();

#ifdef Q_OS_MACOS
  ui_->notifications_options->setEnabled(ui_->notifications_pretty->isChecked());
#endif

  // Pretty OSD
  pretty_popup_->ReloadSettings();
  ui_->notifications_opacity->setValue(static_cast<int>(pretty_popup_->background_opacity() * 100));

  QRgb color = pretty_popup_->background_color();
  if (color == OSDPretty::kPresetBlue) {
    ui_->notifications_bg_preset->setCurrentIndex(0);
  }
  else if (color == OSDPretty::kPresetRed) {
    ui_->notifications_bg_preset->setCurrentIndex(1);
  }
  else {
    ui_->notifications_bg_preset->setCurrentIndex(2);
  }
  ui_->notifications_bg_preset->setItemData(2, QColor(color), Qt::DecorationRole);
  ui_->notifications_disable_duration->setChecked(pretty_popup_->disable_duration());

  ui_->notifications_fading->setChecked(pretty_popup_->fading());

  UpdatePopupVisible();

  Init(ui_->layout_notificationssettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(OSDBase::kSettingsGroup))) set_changed();

}

void NotificationsSettingsPage::Save() {

  Settings s;

  OSDBase::Behaviour osd_behaviour = OSDBase::Behaviour::Disabled;
  if      (ui_->notifications_none->isChecked())   osd_behaviour = OSDBase::Behaviour::Disabled;
  else if (ui_->notifications_native->isChecked()) osd_behaviour = OSDBase::Behaviour::Native;
  else if (ui_->notifications_tray->isChecked())   osd_behaviour = OSDBase::Behaviour::TrayPopup;
  else if (ui_->notifications_pretty->isChecked()) osd_behaviour = OSDBase::Behaviour::Pretty;

  s.beginGroup(OSDBase::kSettingsGroup);
  s.setValue("Behaviour", static_cast<int>(osd_behaviour));
  s.setValue("Timeout", ui_->notifications_duration->value() * 1000);
  s.setValue("ShowOnVolumeChange", ui_->notifications_volume->isChecked());
  s.setValue("ShowOnPlayModeChange", ui_->notifications_play_mode->isChecked());
  s.setValue("ShowOnPausePlayback", ui_->notifications_pause->isChecked());
  s.setValue("ShowOnResumePlayback", ui_->notifications_resume->isChecked());
  s.setValue("ShowArt", ui_->notifications_art->isChecked());
  s.setValue("CustomTextEnabled", ui_->notifications_custom_text_enabled->isChecked());
  s.setValue("CustomText1", ui_->notifications_custom_text1->text());
  s.setValue("CustomText2", ui_->notifications_custom_text2->text());
  s.endGroup();

  s.beginGroup(OSDPretty::kSettingsGroup);
  s.setValue("foreground_color", pretty_popup_->foreground_color());
  s.setValue("background_color", pretty_popup_->background_color());
  s.setValue("background_opacity", pretty_popup_->background_opacity());
  s.setValue("popup_screen", pretty_popup_->popup_screen());
  s.setValue("popup_pos", pretty_popup_->popup_pos());
  s.setValue("font", pretty_popup_->font().toString());
  s.setValue("disable_duration", ui_->notifications_disable_duration->isChecked());
  s.setValue("fading", ui_->notifications_fading->isChecked());
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
      pretty_popup_->set_background_color(OSDPretty::kPresetBlue);
      break;

    case 1:
      pretty_popup_->set_background_color(OSDPretty::kPresetRed);
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

  OSDBase::Behaviour notificationType = OSDBase::Behaviour::Disabled;
  if (ui_->notifications_native->isChecked()) {
    notificationType = OSDBase::Behaviour::Native;
  }
  else if (ui_->notifications_pretty->isChecked()) {
    notificationType = OSDBase::Behaviour::Pretty;
  }
  else if (ui_->notifications_tray->isChecked()) {
    notificationType = OSDBase::Behaviour::TrayPopup;
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
