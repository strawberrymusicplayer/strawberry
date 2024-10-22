/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QWidget>
#include <QSettings>
#include <QToolButton>

#include "core/iconloader.h"
#include "core/settings.h"
#include "constants/appearancesettings.h"
#include "radioviewcontainer.h"
#include "ui_radioviewcontainer.h"

using namespace Qt::Literals::StringLiterals;

RadioViewContainer::RadioViewContainer(QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_RadioViewContainer) {

  ui_->setupUi(this);

  QObject::connect(ui_->refresh, &QToolButton::clicked, this, &RadioViewContainer::Refresh);

  ui_->refresh->setIcon(IconLoader::Load(u"view-refresh"_s));

  ReloadSettings();

}

RadioViewContainer::~RadioViewContainer() { delete ui_; }

void RadioViewContainer::ReloadSettings() {

  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  int iconsize = s.value(AppearanceSettings::kIconSizeLeftPanelButtons, 22).toInt();
  s.endGroup();

  ui_->refresh->setIconSize(QSize(iconsize, iconsize));

}
