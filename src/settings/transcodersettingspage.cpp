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

#include <QShowEvent>

#include "core/iconloader.h"
#include "settingspage.h"
#include "transcoder/transcoderoptionsflac.h"
#include "transcoder/transcoderoptionswavpack.h"
#include "transcoder/transcoderoptionsvorbis.h"
#include "transcoder/transcoderoptionsopus.h"
#include "transcoder/transcoderoptionsspeex.h"
#include "transcoder/transcoderoptionsaac.h"
#include "transcoder/transcoderoptionsasf.h"
#include "transcoder/transcoderoptionsmp3.h"
#include "transcodersettingspage.h"
#include "ui_transcodersettingspage.h"
#include "constants/transcodersettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace TranscoderSettings;

class SettingsDialog;

TranscoderSettingsPage::TranscoderSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_TranscoderSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"tools-wizard"_s, true, 0, 32));

}

TranscoderSettingsPage::~TranscoderSettingsPage() {
  delete ui_;
}

void TranscoderSettingsPage::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) set_changed();

  QWidget::showEvent(e);

}

void TranscoderSettingsPage::Load() {

  ui_->transcoding_flac->Load();
  ui_->transcoding_wavpack->Load();
  ui_->transcoding_vorbis->Load();
  ui_->transcoding_opus->Load();
  ui_->transcoding_speex->Load();
  ui_->transcoding_aac->Load();
  ui_->transcoding_asf->Load();
  ui_->transcoding_mp3->Load();

  Init(ui_->layout_transcodersettingspage->parentWidget());
  if (isVisible()) set_changed();

}

void TranscoderSettingsPage::Save() {

  ui_->transcoding_flac->Save();
  ui_->transcoding_wavpack->Save();
  ui_->transcoding_vorbis->Save();
  ui_->transcoding_opus->Save();
  ui_->transcoding_speex->Save();
  ui_->transcoding_aac->Save();
  ui_->transcoding_asf->Save();
  ui_->transcoding_mp3->Save();

}
