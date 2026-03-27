/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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

#include <QObject>
#include <QSettings>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>

#include "settingsdialog.h"
#include "radiosettingspage.h"
#include "ui_radiosettingspage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "constants/somafmsettings.h"
#include "constants/radiobrowsersettings.h"

using namespace Qt::Literals::StringLiterals;

RadioSettingsPage::RadioSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_RadioSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"radio"_s, true, 0, 32));

  // SomaFM quality options
  ui_->combo_somafm_quality->addItem(tr("Highest"), u"highest"_s);
  ui_->combo_somafm_quality->addItem(tr("High"), u"high"_s);
  ui_->combo_somafm_quality->addItem(tr("Low"), u"low"_s);

  // Radio Browser sort options
  ui_->combo_default_sort->addItem(tr("By votes"), u"votes"_s);
  ui_->combo_default_sort->addItem(tr("By clicks"), u"clickcount"_s);
  ui_->combo_default_sort->addItem(tr("By name"), u"name"_s);
  ui_->combo_default_sort->addItem(tr("By bitrate"), u"bitrate"_s);

  // Radio Browser country options
  ui_->combo_default_country->addItem(tr("All countries"), QString());
  ui_->combo_default_country->addItem(u"Germany"_s, u"Germany"_s);
  ui_->combo_default_country->addItem(u"Austria"_s, u"Austria"_s);
  ui_->combo_default_country->addItem(u"Switzerland"_s, u"Switzerland"_s);
  ui_->combo_default_country->addItem(u"United Kingdom"_s, u"The United Kingdom Of Great Britain And Northern Ireland"_s);
  ui_->combo_default_country->addItem(u"United States"_s, u"The United States Of America"_s);
  ui_->combo_default_country->addItem(u"France"_s, u"France"_s);
  ui_->combo_default_country->addItem(u"Spain"_s, u"Spain"_s);
  ui_->combo_default_country->addItem(u"Italy"_s, u"Italy"_s);
  ui_->combo_default_country->addItem(u"Netherlands"_s, u"The Netherlands"_s);
  ui_->combo_default_country->addItem(u"Japan"_s, u"Japan"_s);
  ui_->combo_default_country->addItem(u"Brazil"_s, u"Brazil"_s);

}

RadioSettingsPage::~RadioSettingsPage() { delete ui_; }

void RadioSettingsPage::Load() {

  // SomaFM
  {
    Settings s;
    s.beginGroup(QLatin1String(SomaFMSettings::kSettingsGroup));
    ComboBoxLoadFromSettings(s, ui_->combo_somafm_quality, QLatin1String(SomaFMSettings::kQuality), QLatin1String(SomaFMSettings::kQualityDefault));
    s.endGroup();
  }

  // Radio Browser
  {
    Settings s;
    s.beginGroup(QLatin1String(RadioBrowserSettings::kSettingsGroup));
    ui_->spin_search_limit->setValue(s.value(QLatin1String(RadioBrowserSettings::kSearchLimit), RadioBrowserSettings::kSearchLimitDefault).toInt());
    ui_->check_hide_broken->setChecked(s.value(QLatin1String(RadioBrowserSettings::kHideBroken), RadioBrowserSettings::kHideBrokenDefault).toBool());
    ComboBoxLoadFromSettings(s, ui_->combo_default_sort, u"default_sort"_s, u"votes"_s);
    ComboBoxLoadFromSettings(s, ui_->combo_default_country, u"default_country"_s, QString());
    s.endGroup();
  }

  Init(ui_->layout_radiosettingspage->parentWidget());

}

void RadioSettingsPage::Save() {

  // SomaFM
  {
    Settings s;
    s.beginGroup(QLatin1String(SomaFMSettings::kSettingsGroup));
    s.setValue(QLatin1String(SomaFMSettings::kQuality), ui_->combo_somafm_quality->currentData().toString());
    s.endGroup();
  }

  // Radio Browser
  {
    Settings s;
    s.beginGroup(QLatin1String(RadioBrowserSettings::kSettingsGroup));
    s.setValue(QLatin1String(RadioBrowserSettings::kSearchLimit), ui_->spin_search_limit->value());
    s.setValue(QLatin1String(RadioBrowserSettings::kHideBroken), ui_->check_hide_broken->isChecked());
    s.setValue(u"default_sort"_s, ui_->combo_default_sort->currentData().toString());
    s.setValue(u"default_country"_s, ui_->combo_default_country->currentData().toString());
    s.endGroup();
  }

}
