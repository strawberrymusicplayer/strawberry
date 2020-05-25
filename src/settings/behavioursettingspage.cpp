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

#include <algorithm>

#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegExp>
#include <QDir>
#include <QLocale>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>

#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "core/utilities.h"
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

#ifdef HAVE_TRANSLATIONS
  // Populate the language combo box.  We do this by looking at all the compiled in translations.
  QDir dir(":/translations/");
  QStringList codes(dir.entryList(QStringList() << "*.qm"));
  QRegExp lang_re("^strawberry_(.*).qm$");
  for (const QString &filename : codes) {

    // The regex captures the "ru" from "strawberry_ru.qm"
    if (!lang_re.exactMatch(filename)) continue;

    QString code = lang_re.cap(1);
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

#ifdef HAVE_X11
  QString de = Utilities::DesktopEnvironment();
  if (de.toLower() == "kde")
#endif
    ui_->checkbox_scrolltrayicon->hide();

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
  ui_->checkbox_resumeplayback->setChecked(s.value("resumeplayback", false).toBool());
  ui_->checkbox_playingwidget->setChecked(s.value("playing_widget", true).toBool());

  MainWindow::StartupBehaviour behaviour = MainWindow::StartupBehaviour(s.value("startupbehaviour", MainWindow::Startup_Remember).toInt());
  switch (behaviour) {
    case MainWindow::Startup_AlwaysHide: ui_->radiobutton_alwayshide->setChecked(true); break;
    case MainWindow::Startup_AlwaysShow: ui_->radiobutton_alwaysshow->setChecked(true); break;
    case MainWindow::Startup_Remember:   ui_->radiobutton_remember->setChecked(true); break;
  }

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

}

void BehaviourSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  s.setValue("showtrayicon", ui_->checkbox_showtrayicon->isChecked());
  s.setValue("keeprunning", ui_->checkbox_keeprunning->isChecked());
  s.setValue("resumeplayback", ui_->checkbox_resumeplayback->isChecked());
  s.setValue("playing_widget", ui_->checkbox_playingwidget->isChecked());
  s.setValue("scrolltrayicon", ui_->checkbox_scrolltrayicon->isChecked());

  MainWindow::StartupBehaviour behaviour = MainWindow::Startup_Remember;
  if (ui_->radiobutton_alwayshide->isChecked()) behaviour = MainWindow::Startup_AlwaysHide;
  if (ui_->radiobutton_alwaysshow->isChecked()) behaviour = MainWindow::Startup_AlwaysShow;
  if (ui_->radiobutton_remember->isChecked()) behaviour = MainWindow::Startup_Remember;
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

  ui_->radiobutton_alwayshide->setEnabled(on);
  if (!on && ui_->radiobutton_alwayshide->isChecked()) ui_->radiobutton_remember->setChecked(true);
  ui_->checkbox_keeprunning->setEnabled(on);
  ui_->checkbox_scrolltrayicon->setEnabled(on);

}
