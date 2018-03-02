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

#include <QDir>

#include "behavioursettingspage.h"

#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "ui_behavioursettingspage.h"

const char *BehaviourSettingsPage::kSettingsGroup = "Behaviour";

BehaviourSettingsPage::BehaviourSettingsPage(SettingsDialog *dialog) : SettingsPage(dialog), ui_(new Ui_BehaviourSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("strawberry"));

  connect(ui_->checkbox_showtrayicon, SIGNAL(toggled(bool)), SLOT(ShowTrayIconToggled(bool)));

#ifdef Q_OS_DARWIN
  ui_->checkbox_showtrayicon->setEnabled(false);
  ui_->startup_group->setEnabled(false);
#endif

}

BehaviourSettingsPage::~BehaviourSettingsPage() {
  delete ui_;
}

void BehaviourSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->checkbox_showtrayicon->setChecked(s.value("showtrayicon", true).toBool());
  ui_->checkbox_scrolltrayicon->setChecked(s.value("scrolltrayicon", ui_->checkbox_showtrayicon->isChecked()).toBool());
  ui_->checkbox_keeprunning->setChecked(s.value("keeprunning", false).toBool());

  MainWindow::StartupBehaviour behaviour = MainWindow::StartupBehaviour(s.value("startupbehaviour", MainWindow::Startup_Remember).toInt());
  switch (behaviour) {
    case MainWindow::Startup_AlwaysHide: ui_->radiobutton_alwayshide->setChecked(true); break;
    case MainWindow::Startup_AlwaysShow: ui_->radiobutton_alwaysshow->setChecked(true); break;
    case MainWindow::Startup_Remember:   ui_->radiobutton_remember->setChecked(true); break;
  }

  ui_->checkbox_resumeplayback->setChecked(s.value("resumeplayback", false).toBool());
  ui_->spinbox_seekstepsec->setValue(s.value("seek_step_sec", 10).toInt());

  s.endGroup();

}

void BehaviourSettingsPage::Save() {
  
  QSettings s;

  MainWindow::StartupBehaviour behaviour = MainWindow::Startup_Remember;
  if (ui_->radiobutton_alwayshide->isChecked()) behaviour = MainWindow::Startup_AlwaysHide;
  if (ui_->radiobutton_alwaysshow->isChecked()) behaviour = MainWindow::Startup_AlwaysShow;
  if (ui_->radiobutton_remember->isChecked()) behaviour = MainWindow::Startup_Remember;

  s.beginGroup(kSettingsGroup);
  s.setValue("showtrayicon", ui_->checkbox_showtrayicon->isChecked());
  s.setValue("scrolltrayicon", ui_->checkbox_scrolltrayicon->isChecked());
  s.setValue("keeprunning", ui_->checkbox_keeprunning->isChecked());
  s.setValue("startupbehaviour", int(behaviour));
  s.setValue("resumeplayback", ui_->checkbox_resumeplayback->isChecked());
  s.endGroup();

}

void BehaviourSettingsPage::ShowTrayIconToggled(bool on) {
 
  ui_->radiobutton_alwayshide->setEnabled(on);
  if (!on && ui_->radiobutton_alwayshide->isChecked()) ui_->radiobutton_remember->setChecked(true);
  ui_->checkbox_keeprunning->setEnabled(on);
  ui_->checkbox_keeprunning->setChecked(on);
  ui_->checkbox_scrolltrayicon->setEnabled(on);

}
