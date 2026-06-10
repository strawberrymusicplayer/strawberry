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
#include <QColor>
#include <QColorDialog>
#include <QPushButton>
#include <QPalette>

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
      ui_(new Ui_WaveformSettingsPage),
      color_is_custom_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"waveform-config"_s, true, 0, 32));

  QObject::connect(ui_->select_waveform_color, &QPushButton::pressed, this, &WaveformSettingsPage::WaveformSelectColor);

  WaveformSettingsPage::Load();

}

WaveformSettingsPage::~WaveformSettingsPage() { delete ui_; }

void WaveformSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->waveform_enabled->setChecked(s.value(kEnabled, false).toBool());
  ui_->waveform_save->setChecked(s.value(kSave, false).toBool());

  current_waveform_color_ = s.value(kColor).value<QColor>();
  s.endGroup();

  if (current_waveform_color_.isValid()) {
    color_is_custom_ = true;
    UpdateColorButtonStyle(ui_->select_waveform_color, current_waveform_color_);
  }
  else {
    color_is_custom_ = false;
    // Show the theme Highlight as a preview so the button is not a blank grey box;
    // this is display-only — kColor is not written until the user explicitly picks.
    UpdateColorButtonStyle(ui_->select_waveform_color, palette().color(QPalette::Highlight));
  }

  Init(ui_->layout_waveformsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void WaveformSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->waveform_enabled->isChecked());
  s.setValue(kSave, ui_->waveform_save->isChecked());
  // Only persist a custom color — when unset the renderer falls back to the theme Highlight.
  if (color_is_custom_) {
    s.setValue(kColor, current_waveform_color_);
  }
  else {
    s.remove(kColor);
  }
  s.endGroup();

  // D-04: enabling waveform writes moodbar show off (single seekbar choice).
  // Guarded: MoodbarSettings is only available when moodbar was compiled in.
#ifdef HAVE_MOODBAR
  if (ui_->waveform_enabled->isChecked()) {
    Settings ms;
    ms.beginGroup(MoodbarSettings::kSettingsGroup);
    ms.setValue(MoodbarSettings::kShow, false);
    ms.endGroup();
  }
#endif

}

void WaveformSettingsPage::Cancel() {}

void WaveformSettingsPage::WaveformSelectColor() {

  QColor c = QColorDialog::getColor(current_waveform_color_.isValid() ? current_waveform_color_ : palette().color(QPalette::Highlight), this);
  if (!c.isValid()) return;

  current_waveform_color_ = c;
  color_is_custom_ = true;
  UpdateColorButtonStyle(ui_->select_waveform_color, current_waveform_color_);
  set_changed();

}

void WaveformSettingsPage::UpdateColorButtonStyle(QWidget *button, const QColor &color) {

  button->setStyleSheet(QStringLiteral("background-color: rgb(%1, %2, %3); color: rgb(255, 255, 255); border: 1px dotted black;").arg(color.red()).arg(color.green()).arg(color.blue()));

}
