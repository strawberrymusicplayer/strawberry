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
#include <utility>

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

#include "constants/behavioursettings.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "settings/settingspage.h"
#include "behavioursettingspage.h"
#include "ui_behavioursettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace BehaviourSettings;

class SettingsDialog;

#ifdef HAVE_TRANSLATIONS
namespace {
bool LocaleAwareCompare(const QString &a, const QString &b) {
  return a.localeAwareCompare(b) < 0;
}
}  // namespace
#endif

BehaviourSettingsPage::BehaviourSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_BehaviourSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"strawberry"_s, true, 0, 32));

  QObject::connect(ui_->checkbox_showtrayicon, &QCheckBox::toggled, this, &BehaviourSettingsPage::ShowTrayIconToggled);

#ifdef Q_OS_MACOS
  ui_->checkbox_showtrayicon->hide();
  ui_->checkbox_trayicon_progress->hide();
  ui_->groupbox_startup->hide();
#endif

#if !defined(HAVE_DBUS) || defined(Q_OS_MACOS)
  ui_->checkbox_taskbar_progress->hide();
#endif

#ifdef HAVE_TRANSLATIONS
  // Populate the language combo box.  We do this by looking at all the compiled in translations.
  QDir dir1(u":/i18n"_s);
  QDir dir2(QStringLiteral(TRANSLATIONS_DIR));
  QStringList codes = dir1.entryList(QStringList() << u"*.qm"_s);
  if (dir2.exists()) {
    codes << dir2.entryList(QStringList() << u"*.qm"_s);
  }
  static const QRegularExpression lang_re(u"^strawberry_(.*).qm$"_s);
  for (const QString &filename : std::as_const(codes)) {

    QRegularExpressionMatch re_match = lang_re.match(filename);

    // The regex captures the "ru" from "strawberry_ru.qm"
    if (!re_match.hasMatch()) continue;

    QString code = re_match.captured(1);
    QString lookup_code = QString(code)
                              .replace("@latin"_L1, "_Latn"_L1)
                              .replace("_CN"_L1, "_Hans_CN"_L1)
                              .replace("_TW"_L1, "_Hant_TW"_L1);

    QString language_name = QLocale::languageToString(QLocale(lookup_code).language());
    QString native_name = QLocale(lookup_code).nativeLanguageName();
    if (!native_name.isEmpty()) {
      language_name = native_name;
    }
    QString name = u"%1 (%2)"_s.arg(language_name, code);

    language_map_[name] = code;
  }

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

  Settings s;
  s.beginGroup(kSettingsGroup);

#ifdef Q_OS_MACOS
  ui_->checkbox_keeprunning->setEnabled(true);
  ui_->checkbox_keeprunning->setChecked(s.value(kKeepRunning, false).toBool());
#else
  const bool systemtray_available = QSystemTrayIcon::isSystemTrayAvailable();
  ui_->checkbox_showtrayicon->setEnabled(systemtray_available);
  ui_->checkbox_showtrayicon->setChecked(systemtray_available && s.value(kShowTrayIcon, true).toBool());
  ui_->checkbox_keeprunning->setEnabled(systemtray_available && ui_->checkbox_showtrayicon->isChecked());
  ui_->checkbox_keeprunning->setChecked(s.value(kKeepRunning, false).toBool());
  ui_->checkbox_trayicon_progress->setEnabled(systemtray_available && ui_->checkbox_showtrayicon->isChecked());
  ui_->checkbox_trayicon_progress->setChecked(systemtray_available && ui_->checkbox_showtrayicon->isChecked() && s.value(kTrayIconProgress, false).toBool());
  ui_->radiobutton_hide->setEnabled(systemtray_available && ui_->checkbox_showtrayicon->isChecked());
#endif

#if defined(HAVE_DBUS) && !defined(Q_OS_MACOS)
  ui_->checkbox_taskbar_progress->setChecked(s.value(kTaskbarProgress, true).toBool());
#endif

  ui_->checkbox_resumeplayback->setChecked(s.value(kResumePlayback, false).toBool());
  ui_->checkbox_playingwidget->setChecked(s.value(kPlayingWidget, true).toBool());

#ifndef Q_OS_MACOS
  const StartupBehaviour startup_behaviour = static_cast<StartupBehaviour>(s.value(kStartupBehaviour, static_cast<int>(StartupBehaviour::Remember)).toInt());
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
      if (systemtray_available && ui_->checkbox_showtrayicon->isChecked()) {
        ui_->radiobutton_hide->setChecked(true);
        break;
      }
      ;
      [[fallthrough]];
    case StartupBehaviour::Remember:
      ui_->radiobutton_remember->setChecked(true);
      break;
  }
#endif

  QString name = language_map_.key(s.value(kLanguage).toString());
  if (name.isEmpty()) {
    ui_->combobox_language->setCurrentIndex(0);
  }
  else {
    ui_->combobox_language->setCurrentIndex(ui_->combobox_language->findText(name));
  }

  ui_->combobox_menuplaymode->setCurrentIndex(ui_->combobox_menuplaymode->findData(s.value(kMenuPlayMode, static_cast<int>(PlayBehaviour::Never)).toInt()));

  ui_->combobox_previousmode->setCurrentIndex(ui_->combobox_previousmode->findData(s.value(kMenuPreviousMode, static_cast<int>(PreviousBehaviour::DontRestart)).toInt()));

  ui_->combobox_doubleclickaddmode->setCurrentIndex(ui_->combobox_doubleclickaddmode->findData(s.value(kDoubleClickAddMode, static_cast<int>(AddBehaviour::Append)).toInt()));

  ui_->combobox_doubleclickplaymode->setCurrentIndex(ui_->combobox_doubleclickplaymode->findData(s.value(kDoubleClickPlayMode, static_cast<int>(PlayBehaviour::Never)).toInt()));

  ui_->combobox_doubleclickplaylistaddmode->setCurrentIndex(ui_->combobox_doubleclickplaylistaddmode->findData(s.value(kDoubleClickPlaylistAddMode, static_cast<int>(PlaylistAddBehaviour::Play)).toInt()));

  ui_->spinbox_seekstepsec->setValue(s.value(kSeekStepSec, 10).toInt());

  ui_->spinbox_volumeincrement->setValue(s.value(kVolumeIncrement, 5).toInt());

  s.endGroup();

  Init(ui_->layout_behavioursettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void BehaviourSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  s.setValue(kShowTrayIcon, ui_->checkbox_showtrayicon->isChecked());
  s.setValue(kKeepRunning, ui_->checkbox_keeprunning->isChecked());
  s.setValue(kTrayIconProgress, ui_->checkbox_trayicon_progress->isChecked());
#if defined(HAVE_DBUS) && !defined(Q_OS_MACOS)
  s.setValue(kTaskbarProgress, ui_->checkbox_taskbar_progress->isChecked());
#endif
  s.setValue(kResumePlayback, ui_->checkbox_resumeplayback->isChecked());
  s.setValue(kPlayingWidget, ui_->checkbox_playingwidget->isChecked());

  StartupBehaviour startup_behaviour = StartupBehaviour::Remember;
  if (ui_->radiobutton_remember->isChecked()) startup_behaviour = StartupBehaviour::Remember;
  if (ui_->radiobutton_show->isChecked()) startup_behaviour = StartupBehaviour::Show;
  if (ui_->radiobutton_hide->isChecked()) startup_behaviour = StartupBehaviour::Hide;
  if (ui_->radiobutton_show_maximized->isChecked()) startup_behaviour = StartupBehaviour::ShowMaximized;
  if (ui_->radiobutton_show_minimized->isChecked()) startup_behaviour = StartupBehaviour::ShowMinimized;
  s.setValue(kStartupBehaviour, static_cast<int>(startup_behaviour));

  s.setValue(kLanguage, language_map_.contains(ui_->combobox_language->currentText()) ? language_map_[ui_->combobox_language->currentText()] : QString());

  const PlayBehaviour menu_playmode = static_cast<PlayBehaviour>(ui_->combobox_menuplaymode->currentData().toInt());

  const PreviousBehaviour menu_previousmode = static_cast<PreviousBehaviour>(ui_->combobox_previousmode->currentData().toInt());
  const AddBehaviour doubleclick_addmode = static_cast<AddBehaviour>(ui_->combobox_doubleclickaddmode->currentData().toInt());

  const PlayBehaviour doubleclick_playmode = static_cast<PlayBehaviour>(ui_->combobox_doubleclickplaymode->currentData().toInt());

  const PlaylistAddBehaviour doubleclick_playlist_addmode = static_cast<PlaylistAddBehaviour>(ui_->combobox_doubleclickplaylistaddmode->currentData().toInt());

  s.setValue(kMenuPlayMode, static_cast<int>(menu_playmode));
  s.setValue(kMenuPreviousMode, static_cast<int>(menu_previousmode));
  s.setValue(kDoubleClickAddMode, static_cast<int>(doubleclick_addmode));
  s.setValue(kDoubleClickPlayMode, static_cast<int>(doubleclick_playmode));
  s.setValue(kDoubleClickPlaylistAddMode, static_cast<int>(doubleclick_playlist_addmode));

  s.setValue(kSeekStepSec, ui_->spinbox_seekstepsec->value());

  s.setValue(kVolumeIncrement, ui_->spinbox_volumeincrement->value());

  s.endGroup();

}

void BehaviourSettingsPage::ShowTrayIconToggled(const bool on) {

  ui_->radiobutton_hide->setEnabled(on);
  if (!on && ui_->radiobutton_hide->isChecked()) ui_->radiobutton_remember->setChecked(true);
  ui_->checkbox_keeprunning->setEnabled(on);
  ui_->checkbox_trayicon_progress->setEnabled(on);

}
