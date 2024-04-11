/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QWidget>
#include <QString>
#include <QSettings>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionswavpack.h"
#include "ui_transcoderoptionswavpack.h"

#include "core/settings.h"

namespace {
constexpr char kSettingsGroup[] = "Transcoder/wavpackenc";
}

TranscoderOptionsWavPack::TranscoderOptionsWavPack(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsWavPack) {
  ui_->setupUi(this);
}

TranscoderOptionsWavPack::~TranscoderOptionsWavPack() {
  delete ui_;
}

void TranscoderOptionsWavPack::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  s.endGroup();

}

void TranscoderOptionsWavPack::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  s.endGroup();

}
