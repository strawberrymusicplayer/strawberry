/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#include <QCheckBox>

#include "core/iconloader.h"
#include "core/settings.h"

#include "settingsdialog.h"
#include "settingspage.h"

#ifdef HAVE_MOODBAR
#  include "constants/moodbarsettings.h"
#endif

#include "constants/waveformsettings.h"
#include "waveformsettingspage.h"
#include "ui_waveformsettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace WaveformSettings;

WaveformSettingsPage::WaveformSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_WaveformSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"waveform-config"_s, true, 0, 32));

  WaveformSettingsPage::Load();

}

WaveformSettingsPage::~WaveformSettingsPage() { delete ui_; }

void WaveformSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->waveform_enabled->setChecked(s.value(kEnabled, false).toBool());
  ui_->waveform_show->setChecked(s.value(kShow, false).toBool());
  ui_->waveform_save->setChecked(s.value(kSave, false).toBool());
  s.endGroup();

  Init(ui_->layout_waveformsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void WaveformSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->waveform_enabled->isChecked());
  s.setValue(kShow, ui_->waveform_show->isChecked());
  s.setValue(kSave, ui_->waveform_save->isChecked());
  s.endGroup();

  // D-04: enabling waveform show writes moodbar show off (single seekbar choice).
  // Guarded: MoodbarSettings is only available when moodbar was compiled in.
#ifdef HAVE_MOODBAR
  if (ui_->waveform_show->isChecked()) {
    Settings ms;
    ms.beginGroup(MoodbarSettings::kSettingsGroup);
    ms.setValue(MoodbarSettings::kShow, false);
    ms.endGroup();
  }
#endif

}

void WaveformSettingsPage::Cancel() {}
