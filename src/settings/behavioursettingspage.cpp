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

#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
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
  setWindowIcon(IconLoader::Load("strawberry"));

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

  ui_->combobox_menuplaymode->setItemData(0, PlayBehaviour_Never);
  ui_->combobox_menuplaymode->setItemData(1, PlayBehaviour_IfStopped);
  ui_->combobox_menuplaymode->setItemData(2, PlayBehaviour_Always);

  ui_->combobox_previousmode->setItemData(0, PreviousBehaviour_DontRestart);
  ui_->combobox_previousmode->setItemData(1, PreviousBehaviour_Restart);

  ui_->combobox_doubleclickaddmode->setItemData(0, AddBehaviour_Append);
  ui_->combobox_doubleclickaddmode->setItemData(1, AddBehaviour_Load);
  ui_->combobox_doubleclickaddmode->setItemData(2, AddBehaviour_OpenInNew);
  ui_->combobox_doubleclickaddmode->setItemData(3, AddBehaviour_Enqueue);

  ui_->combobox_doubleclickplaymode->setItemData(0, PlayBehaviour_Never);
  ui_->combobox_doubleclickplaymode->setItemData(1, PlayBehaviour_IfStopped);
  ui_->combobox_doubleclickplaymode->setItemData(2, PlayBehaviour_Always);

  ui_->combobox_doubleclickplaylistaddmode->setItemData(0, PlaylistAddBehaviour_Play);
  ui_->combobox_doubleclickplaylistaddmode->setItemData(1, PlaylistAddBehaviour_Enqueue);

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
  ui_->checkbox_artistbio->setChecked(s.value("artistbio", false).toBool());

#ifndef Q_OS_MACOS
  StartupBehaviour behaviour = StartupBehaviour(s.value("startupbehaviour", Startup_Remember).toInt());
  switch (behaviour) {
    case Startup_Show:
      ui_->radiobutton_show->setChecked(true);
      break;
    case Startup_ShowMaximized:
      ui_->radiobutton_show_maximized->setChecked(true);
      break;
    case Startup_ShowMinimized:
      ui_->radiobutton_show_minimized->setChecked(true);
      break;
    case Startup_Hide:
      if (systemtray_available_) {
        ui_->radiobutton_hide->setChecked(true);
        break;
      }
      ;
      // fallthrough
    case BehaviourSettingsPage::Startup_Remember:
    default:
      ui_->radiobutton_remember->setChecked(true);
      break;
  }
#endif

  QString name = language_map_.key(s.value("language").toString());
  if (name.isEmpty())
    ui_->combobox_language->setCurrentIndex(0);
  else
    ui_->combobox_language->setCurrentIndex(ui_->combobox_language->findText(name));

  ui_->combobox_menuplaymode->setCurrentIndex(ui_->combobox_menuplaymode->findData(s.value("menu_playmode", PlayBehaviour_Never).toInt()));

  ui_->combobox_previousmode->setCurrentIndex(ui_->combobox_previousmode->findData(s.value("menu_previousmode", PreviousBehaviour_DontRestart).toInt()));

  ui_->combobox_doubleclickaddmode->setCurrentIndex(ui_->combobox_doubleclickaddmode->findData(s.value("doubleclick_addmode", AddBehaviour_Append).toInt()));

  ui_->combobox_doubleclickplaymode->setCurrentIndex(ui_->combobox_doubleclickplaymode->findData(s.value("doubleclick_playmode", PlayBehaviour_Never).toInt()));

  ui_->combobox_doubleclickplaylistaddmode->setCurrentIndex(ui_->combobox_doubleclickplaylistaddmode->findData(s.value("doubleclick_playlist_addmode", PlaylistAddBehaviour_Play).toInt()));

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
  s.setValue("artistbio", ui_->checkbox_artistbio->isChecked());

  StartupBehaviour behaviour = Startup_Remember;
  if (ui_->radiobutton_remember->isChecked()) behaviour = Startup_Remember;
  if (ui_->radiobutton_show->isChecked()) behaviour = Startup_Show;
  if (ui_->radiobutton_hide->isChecked()) behaviour = Startup_Hide;
  if (ui_->radiobutton_show_maximized->isChecked()) behaviour = Startup_ShowMaximized;
  if (ui_->radiobutton_show_minimized->isChecked()) behaviour = Startup_ShowMinimized;
  s.setValue("startupbehaviour", int(behaviour));

  s.setValue("language", language_map_.contains(ui_->combobox_language->currentText()) ? language_map_[ui_->combobox_language->currentText()] : QString());

  PlayBehaviour menu_playmode = PlayBehaviour(ui_->combobox_menuplaymode->itemData(ui_->combobox_menuplaymode->currentIndex()).toInt());

  PreviousBehaviour menu_previousmode = PreviousBehaviour(ui_->combobox_previousmode->itemData(ui_->combobox_previousmode->currentIndex()).toInt());
  AddBehaviour doubleclick_addmode = AddBehaviour(ui_->combobox_doubleclickaddmode->itemData(ui_->combobox_doubleclickaddmode->currentIndex()).toInt());

  PlayBehaviour doubleclick_playmode = PlayBehaviour(ui_->combobox_doubleclickplaymode->itemData(ui_->combobox_doubleclickplaymode->currentIndex()).toInt());

  PlaylistAddBehaviour doubleclick_playlist_addmode = PlaylistAddBehaviour(ui_->combobox_doubleclickplaylistaddmode->itemData(ui_->combobox_doubleclickplaylistaddmode->currentIndex()).toInt());

  s.setValue("menu_playmode", menu_playmode);
  s.setValue("menu_previousmode", menu_previousmode);
  s.setValue("doubleclick_addmode", doubleclick_addmode);
  s.setValue("doubleclick_playmode", doubleclick_playmode);
  s.setValue("doubleclick_playlist_addmode", doubleclick_playlist_addmode);

  s.setValue("seek_step_sec", ui_->spinbox_seekstepsec->value());

  s.endGroup();

}

void BehaviourSettingsPage::ShowTrayIconToggled(bool on) {

  ui_->radiobutton_hide->setEnabled(on);
  if (!on && ui_->radiobutton_hide->isChecked()) ui_->radiobutton_remember->setChecked(true);
  ui_->checkbox_keeprunning->setEnabled(on);
  ui_->checkbox_trayicon_progress->setEnabled(on);

}
