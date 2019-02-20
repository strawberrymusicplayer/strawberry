/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QVariant>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>

#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "settings/settingspage.h"
#include "behavioursettingspage.h"
#include "ui_behavioursettingspage.h"

class SettingsDialog;

const char *BehaviourSettingsPage::kSettingsGroup = "Behaviour";

BehaviourSettingsPage::BehaviourSettingsPage(SettingsDialog *dialog) : SettingsPage(dialog), ui_(new Ui_BehaviourSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("strawberry"));

  connect(ui_->checkbox_showtrayicon, SIGNAL(toggled(bool)), SLOT(ShowTrayIconToggled(bool)));

#ifdef Q_OS_MACOS
  ui_->checkbox_showtrayicon->setEnabled(false);
  ui_->groupbox_startup->setEnabled(false);
#else
  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    ui_->checkbox_showtrayicon->setEnabled(true);
    ui_->groupbox_startup->setEnabled(true);
  }
  else {
    ui_->checkbox_showtrayicon->setEnabled(false);
    ui_->groupbox_startup->setEnabled(false);
  }
#endif

  ui_->combobox_doubleclickaddmode->setItemData(0, MainWindow::AddBehaviour_Append);
  ui_->combobox_doubleclickaddmode->setItemData(1, MainWindow::AddBehaviour_Load);
  ui_->combobox_doubleclickaddmode->setItemData(2, MainWindow::AddBehaviour_OpenInNew);
  ui_->combobox_doubleclickaddmode->setItemData(3, MainWindow::AddBehaviour_Enqueue);

  ui_->combobox_doubleclickplaymode->setItemData(0, MainWindow::PlayBehaviour_Never);
  ui_->combobox_doubleclickplaymode->setItemData(1, MainWindow::PlayBehaviour_IfStopped);
  ui_->combobox_doubleclickplaymode->setItemData(2, MainWindow::PlayBehaviour_Always);

  ui_->combobox_menuplaymode->setItemData(0, MainWindow::PlayBehaviour_Never);
  ui_->combobox_menuplaymode->setItemData(1, MainWindow::PlayBehaviour_IfStopped);
  ui_->combobox_menuplaymode->setItemData(2, MainWindow::PlayBehaviour_Always);

}

BehaviourSettingsPage::~BehaviourSettingsPage() {
  delete ui_;
}

void BehaviourSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
#ifdef Q_OS_MACOS
  ui_->checkbox_showtrayicon->setChecked(false);
  ui_->checkbox_scrolltrayicon->setChecked(false);
  ui_->checkbox_keeprunning->setChecked(false);
#else
  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    ui_->checkbox_showtrayicon->setChecked(s.value("showtrayicon", true).toBool());
    ui_->checkbox_scrolltrayicon->setChecked(s.value("scrolltrayicon", ui_->checkbox_showtrayicon->isChecked()).toBool());
    ui_->checkbox_keeprunning->setChecked(s.value("keeprunning", false).toBool());
  }
  else {
    ui_->checkbox_showtrayicon->setChecked(false);
    ui_->checkbox_scrolltrayicon->setChecked(false);
    ui_->checkbox_keeprunning->setChecked(false);
  }
#endif

  MainWindow::StartupBehaviour behaviour = MainWindow::StartupBehaviour(s.value("startupbehaviour", MainWindow::Startup_Remember).toInt());
  switch (behaviour) {
    case MainWindow::Startup_AlwaysHide: ui_->radiobutton_alwayshide->setChecked(true); break;
    case MainWindow::Startup_AlwaysShow: ui_->radiobutton_alwaysshow->setChecked(true); break;
    case MainWindow::Startup_Remember:   ui_->radiobutton_remember->setChecked(true); break;
  }

  ui_->checkbox_resumeplayback->setChecked(s.value("resumeplayback", false).toBool());

  ui_->combobox_doubleclickaddmode->setCurrentIndex(ui_->combobox_doubleclickaddmode->findData(s.value("doubleclick_addmode", MainWindow::AddBehaviour_Append).toInt()));
  ui_->combobox_doubleclickplaymode->setCurrentIndex(ui_->combobox_doubleclickplaymode->findData(s.value("doubleclick_playmode", MainWindow::PlayBehaviour_Never).toInt()));
  ui_->combobox_menuplaymode->setCurrentIndex(ui_->combobox_menuplaymode->findData(s.value("menu_playmode", MainWindow::PlayBehaviour_Never).toInt()));

  ui_->spinbox_seekstepsec->setValue(s.value("seek_step_sec", 10).toInt());

  s.endGroup();

}

void BehaviourSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  MainWindow::StartupBehaviour behaviour = MainWindow::Startup_Remember;
  if (ui_->radiobutton_alwayshide->isChecked()) behaviour = MainWindow::Startup_AlwaysHide;
  if (ui_->radiobutton_alwaysshow->isChecked()) behaviour = MainWindow::Startup_AlwaysShow;
  if (ui_->radiobutton_remember->isChecked()) behaviour = MainWindow::Startup_Remember;

  MainWindow::AddBehaviour doubleclick_addmode = MainWindow::AddBehaviour(ui_->combobox_doubleclickaddmode->itemData(ui_->combobox_doubleclickaddmode->currentIndex()).toInt());
  MainWindow::PlayBehaviour doubleclick_playmode = MainWindow::PlayBehaviour(ui_->combobox_doubleclickplaymode->itemData(ui_->combobox_doubleclickplaymode->currentIndex()).toInt());
  MainWindow::PlayBehaviour menu_playmode = MainWindow::PlayBehaviour(ui_->combobox_menuplaymode->itemData(ui_->combobox_menuplaymode->currentIndex()).toInt());

  s.setValue("showtrayicon", ui_->checkbox_showtrayicon->isChecked());
  s.setValue("scrolltrayicon", ui_->checkbox_scrolltrayicon->isChecked());
  s.setValue("keeprunning", ui_->checkbox_keeprunning->isChecked());
  s.setValue("resumeplayback", ui_->checkbox_resumeplayback->isChecked());
  s.setValue("startupbehaviour", int(behaviour));
  s.setValue("doubleclick_addmode", doubleclick_addmode);
  s.setValue("doubleclick_playmode", doubleclick_playmode);
  s.setValue("menu_playmode", menu_playmode);
  s.setValue("seek_step_sec", ui_->spinbox_seekstepsec->value());
  s.endGroup();

}

void BehaviourSettingsPage::ShowTrayIconToggled(bool on) {

  ui_->radiobutton_alwayshide->setEnabled(on);
  if (!on && ui_->radiobutton_alwayshide->isChecked()) ui_->radiobutton_remember->setChecked(true);
  ui_->checkbox_keeprunning->setEnabled(on);
  ui_->checkbox_scrolltrayicon->setEnabled(on);

}
