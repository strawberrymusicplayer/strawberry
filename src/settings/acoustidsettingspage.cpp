/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>

#include "settingsdialog.h"
#include "acoustidsettingspage.h"
#include "ui_acoustidsettingspage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "constants/acoustidsettings.h"
#include "tagfetcher/acoustidclient.h"

using namespace Qt::Literals::StringLiterals;

AcoustidSettingsPage::AcoustidSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_AcoustidSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"edit-find"_s, true, 0, 32));

  QObject::connect(ui_->checkbox_use_custom_api_key, &QCheckBox::toggled, ui_->api_key, &QLineEdit::setEnabled);
  QObject::connect(ui_->checkbox_use_custom_api_key, &QCheckBox::toggled, ui_->label_api_key, &QLabel::setEnabled);

}

AcoustidSettingsPage::~AcoustidSettingsPage() { delete ui_; }

void AcoustidSettingsPage::Load() {

  Settings s;
  s.beginGroup(QLatin1String(AcoustidSettings::kSettingsGroup));

  ui_->api_key->setText(s.value(AcoustidSettings::kApiKey).toString());

  if (AcoustidClient::HasCompiledApiKey()) {
    ui_->checkbox_use_custom_api_key->setVisible(true);
    const bool use_custom_api_key = s.value(AcoustidSettings::kUseCustomApiKey, false).toBool();
    ui_->checkbox_use_custom_api_key->setChecked(use_custom_api_key);
    ui_->api_key->setEnabled(use_custom_api_key);
    ui_->label_api_key->setEnabled(use_custom_api_key);
  }
  else {
    ui_->checkbox_use_custom_api_key->setVisible(false);
    ui_->api_key->setEnabled(true);
    ui_->label_api_key->setEnabled(true);
  }

  s.endGroup();

  Init(ui_->layout_acoustidsettingspage->parentWidget());

}

void AcoustidSettingsPage::Save() {

  Settings s;
  s.beginGroup(QLatin1String(AcoustidSettings::kSettingsGroup));
  s.setValue(AcoustidSettings::kUseCustomApiKey, ui_->checkbox_use_custom_api_key->isChecked());
  s.setValue(AcoustidSettings::kApiKey, ui_->api_key->text());
  s.endGroup();

}
