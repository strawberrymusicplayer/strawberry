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

#include <algorithm>

#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QDir>
#include <QLocale>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QStandardPaths>

#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "settings/settingspage.h"
#include "behavioursettingspage.h"
#include "ui_behavioursettingspage.h"

class SettingsDialog;

const char *BehaviourSettingsPage::kSettingsGroup = "Behaviour";

#ifdef HAVE_TRANSLATIONS
namespace {
bool LocaleAwareCompare(const QString &a, const QString &b) {
  return a.localeAwareCompare(b) < 0;
}
}  // namespace
#endif

BehaviourSettingsPage::BehaviourSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_BehaviourSettingsPage),
      systemtray_available_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("strawberry", true, 0, 32));

  systemtray_available_ = QSystemTrayIcon::isSystemTrayAvailable();

  QObject::connect(ui_->checkbox_showtrayicon, &QCheckBox::toggled, this, &BehaviourSettingsPage::ShowTrayIconToggled);

#ifdef Q_OS_MACOS
  ui_->checkbox_showtrayicon->hide();
  ui_->groupbox_startup->hide();
#endif

#ifdef HAVE_TRANSLATIONS
  // Populate the language combo box.  We do this by looking at all the compiled in translations.
  QDir dir1(":/translations/");
  QDir dir2(TRANSLATIONS_DIR);
  QStringList codes(dir1.entryList(QStringList() << "*.qm"));
  if (dir2.exists()) {
    codes << dir2.entryList(QStringList() << "*.qm");
  }
  QRegularExpression lang_re("^strawberry_(.*).qm$");
  for (const QString &filename : codes) {

    QRegularExpressionMatch re_match = lang_re.match(filename);

    // The regex captures the "ru" from "strawberry_ru.qm"
    if (!re_match.hasMatch()) continue;

    QString code = re_match.captured(1);
    QString lookup_code = QString(code)
                              .replace("@latin", "_Latn")
                              .replace("_CN", "_Hans_CN")
                              .replace("_TW", "_Hant_TW");

    QString language_name = QLocale::languageToString(QLocale(lookup_code).language());
    QString native_name = QLocale(lookup_code).nativeLanguageName();
    if (!native_name.isEmpty()) {
      language_name = native_name;
    }
    QString name = QString("%1 (%2)").arg(language_name, code);

    language_map_[name] = code;
  }

  language_map_["English (en)"] = "en";

  // Sort the names and show them in the UI
  QStringList names = language_map_.keys();
  std::stable_sort(names.begin(), names.end(), LocaleAwareCompare);
  ui_->combobox_language->addItems(names);
#else
  ui_->groupbox_language->setEnabled(false);
  ui_->groupbox_language->setVisible(false);
#endif

  ui_->combobox_menuplaymode->setItemData(0, static_cast<int>(PlayBehaviour::Never));
  ui_->combobox_menuplaymode->setItemData(1, static_cast<int>(PlayBehaviour::IfStopped));
  ui_->combobox_menuplaymode->setItemData(2, static_cast<int>(PlayBehaviour::Always));

  ui_->combobox_previousmode->setItemData(0, static_cast<int>(PreviousBehaviour::DontRestart));
  ui_->combobox_previousmode->setItemData(1, static_cast<int>(PreviousBehaviour::Restart));

  ui_->combobox_doubleclickaddmode->setItemData(0, static_cast<int>(AddBehaviour::Append));
  ui_->combobox_doubleclickaddmode->setItemData(1, static_cast<int>(AddBehaviour::Load));
  ui_->combobox_doubleclickaddmode->setItemData(2, static_cast<int>(AddBehaviour::OpenInNew));
  ui_->combobox_doubleclickaddmode->setItemData(3, static_cast<int>(AddBehaviour::Enqueue));

  ui_->combobox_doubleclickplaymode->setItemData(0, static_cast<int>(PlayBehaviour::Never));
  ui_->combobox_doubleclickplaymode->setItemData(1, static_cast<int>(PlayBehaviour::IfStopped));
  ui_->combobox_doubleclickplaymode->setItemData(2, static_cast<int>(PlayBehaviour::Always));

  ui_->combobox_doubleclickplaylistaddmode->setItemData(0, static_cast<int>(PlaylistAddBehaviour::Play));
  ui_->combobox_doubleclickplaylistaddmode->setItemData(1, static_cast<int>(PlaylistAddBehaviour::Enqueue));

}

BehaviourSettingsPage::~BehaviourSettingsPage() {
  delete ui_;
}

void BehaviourSettingsPage::Load() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

#ifndef Q_OS_MACOS
  if (systemtray_available_) {
    ui_->checkbox_showtrayicon->setEnabled(true);
    ui_->checkbox_showtrayicon->setChecked(s.value("showtrayicon", true).toBool());
    ui_->radiobutton_hide->setEnabled(true);
  }
  else {
    ui_->checkbox_showtrayicon->setEnabled(false);
    ui_->checkbox_showtrayicon->setChecked(false);
    ui_->radiobutton_hide->setEnabled(false);
    ui_->radiobutton_hide->setChecked(false);
  }
#endif

  if (systemtray_available_) {
    ui_->checkbox_keeprunning->setEnabled(true);
    ui_->checkbox_keeprunning->setChecked(s.value("keeprunning", false).toBool());
    ui_->checkbox_trayicon_progress->setEnabled(true);
    ui_->checkbox_trayicon_progress->setChecked(s.value("trayicon_progress", false).toBool());
  }
  else {
    ui_->checkbox_keeprunning->setEnabled(false);
    ui_->checkbox_keeprunning->setChecked(false);
    ui_->checkbox_trayicon_progress->setEnabled(false);
    ui_->checkbox_trayicon_progress->setChecked(false);
  }

  ui_->checkbox_resumeplayback->setChecked(s.value("resumeplayback", false).toBool());
  ui_->checkbox_playingwidget->setChecked(s.value("playing_widget", true).toBool());

#ifndef Q_OS_MACOS
  const StartupBehaviour startup_behaviour = static_cast<StartupBehaviour>(s.value("startupbehaviour", static_cast<int>(StartupBehaviour::Remember)).toInt());
  switch (startup_behaviour) {
    case StartupBehaviour::Show:
      ui_->radiobutton_show->setChecked(true);
      break;
    case StartupBehaviour::ShowMaximized:
      ui_->radiobutton_show_maximized->setChecked(true);
      break;
    case StartupBehaviour::ShowMinimized:
      ui_->radiobutton_show_minimized->setChecked(true);
      break;
    case StartupBehaviour::Hide:
      if (systemtray_available_) {
        ui_->radiobutton_hide->setChecked(true);
        break;
      }
      ;
      [[fallthrough]];
    case BehaviourSettingsPage::StartupBehaviour::Remember:
      ui_->radiobutton_remember->setChecked(true);
      break;
  }
#endif

  QString name = language_map_.key(s.value("language").toString());
  if (name.isEmpty()) {
    ui_->combobox_language->setCurrentIndex(0);
  }
  else {
    ui_->combobox_language->setCurrentIndex(ui_->combobox_language->findText(name));
  }

  ui_->combobox_menuplaymode->setCurrentIndex(ui_->combobox_menuplaymode->findData(s.value("menu_playmode", static_cast<int>(PlayBehaviour::Never)).toInt()));

  ui_->combobox_previousmode->setCurrentIndex(ui_->combobox_previousmode->findData(s.value("menu_previousmode", static_cast<int>(PreviousBehaviour::DontRestart)).toInt()));

  ui_->combobox_doubleclickaddmode->setCurrentIndex(ui_->combobox_doubleclickaddmode->findData(s.value("doubleclick_addmode", static_cast<int>(AddBehaviour::Append)).toInt()));

  ui_->combobox_doubleclickplaymode->setCurrentIndex(ui_->combobox_doubleclickplaymode->findData(s.value("doubleclick_playmode", static_cast<int>(PlayBehaviour::Never)).toInt()));

  ui_->combobox_doubleclickplaylistaddmode->setCurrentIndex(ui_->combobox_doubleclickplaylistaddmode->findData(s.value("doubleclick_playlist_addmode", static_cast<int>(PlaylistAddBehaviour::Play)).toInt()));

  ui_->spinbox_seekstepsec->setValue(s.value("seek_step_sec", 10).toInt());

  s.endGroup();

  Init(ui_->layout_behavioursettingspage->parentWidget());

  if (!QSettings().childGroups().contains(kSettingsGroup)) set_changed();

}

void BehaviourSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  s.setValue("showtrayicon", ui_->checkbox_showtrayicon->isChecked());
  s.setValue("keeprunning", ui_->checkbox_keeprunning->isChecked());
  s.setValue("trayicon_progress", ui_->checkbox_trayicon_progress->isChecked());
  s.setValue("resumeplayback", ui_->checkbox_resumeplayback->isChecked());
  s.setValue("playing_widget", ui_->checkbox_playingwidget->isChecked());

  StartupBehaviour startup_behaviour = StartupBehaviour::Remember;
  if (ui_->radiobutton_remember->isChecked()) startup_behaviour = StartupBehaviour::Remember;
  if (ui_->radiobutton_show->isChecked()) startup_behaviour = StartupBehaviour::Show;
  if (ui_->radiobutton_hide->isChecked()) startup_behaviour = StartupBehaviour::Hide;
  if (ui_->radiobutton_show_maximized->isChecked()) startup_behaviour = StartupBehaviour::ShowMaximized;
  if (ui_->radiobutton_show_minimized->isChecked()) startup_behaviour = StartupBehaviour::ShowMinimized;
  s.setValue("startupbehaviour", static_cast<int>(startup_behaviour));

  s.setValue("language", language_map_.contains(ui_->combobox_language->currentText()) ? language_map_[ui_->combobox_language->currentText()] : QString());

  const PlayBehaviour menu_playmode = static_cast<PlayBehaviour>(ui_->combobox_menuplaymode->currentData().toInt());

  const PreviousBehaviour menu_previousmode = static_cast<PreviousBehaviour>(ui_->combobox_previousmode->currentData().toInt());
  const AddBehaviour doubleclick_addmode = static_cast<AddBehaviour>(ui_->combobox_doubleclickaddmode->currentData().toInt());

  const PlayBehaviour doubleclick_playmode = static_cast<PlayBehaviour>(ui_->combobox_doubleclickplaymode->currentData().toInt());

  const PlaylistAddBehaviour doubleclick_playlist_addmode = static_cast<PlaylistAddBehaviour>(ui_->combobox_doubleclickplaylistaddmode->currentData().toInt());

  s.setValue("menu_playmode", static_cast<int>(menu_playmode));
  s.setValue("menu_previousmode", static_cast<int>(menu_previousmode));
  s.setValue("doubleclick_addmode", static_cast<int>(doubleclick_addmode));
  s.setValue("doubleclick_playmode", static_cast<int>(doubleclick_playmode));
  s.setValue("doubleclick_playlist_addmode", static_cast<int>(doubleclick_playlist_addmode));

  s.setValue("seek_step_sec", ui_->spinbox_seekstepsec->value());

  s.endGroup();

}

void BehaviourSettingsPage::ShowTrayIconToggled(bool on) {

  ui_->radiobutton_hide->setEnabled(on);
  if (!on && ui_->radiobutton_hide->isChecked()) ui_->radiobutton_remember->setChecked(true);
  ui_->checkbox_keeprunning->setEnabled(on);
  ui_->checkbox_trayicon_progress->setEnabled(on);

}
