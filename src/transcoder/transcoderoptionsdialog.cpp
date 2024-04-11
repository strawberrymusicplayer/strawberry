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

#include <QtGlobal>
#include <QWidget>
#include <QDialog>
#include <QString>
#include <QBoxLayout>
#include <QLayout>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsdialog.h"
#include "transcoderoptionsflac.h"
#include "transcoderoptionswavpack.h"
#include "transcoderoptionsvorbis.h"
#include "transcoderoptionsopus.h"
#include "transcoderoptionsspeex.h"
#include "transcoderoptionsasf.h"
#include "transcoderoptionsaac.h"
#include "transcoderoptionsmp3.h"
#include "ui_transcoderoptionsdialog.h"

TranscoderOptionsDialog::TranscoderOptionsDialog(Song::FileType type, QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_TranscoderOptionsDialog),
      options_(nullptr) {

  ui_->setupUi(this);

  switch (type) {
    case Song::FileType::FLAC:
    case Song::FileType::OggFlac:   options_ = new TranscoderOptionsFLAC(this);     break;
    case Song::FileType::WavPack:   options_ = new TranscoderOptionsWavPack(this);  break;
    case Song::FileType::OggVorbis: options_ = new TranscoderOptionsVorbis(this);   break;
    case Song::FileType::OggOpus:   options_ = new TranscoderOptionsOpus(this);     break;
    case Song::FileType::OggSpeex:  options_ = new TranscoderOptionsSpeex(this);    break;
    case Song::FileType::MP4:       options_ = new TranscoderOptionsAAC(this);      break;
    case Song::FileType::MPEG:      options_ = new TranscoderOptionsMP3(this);      break;
    case Song::FileType::ASF:       options_ = new TranscoderOptionsASF(this);      break;
    default:
      break;
  }

  if (options_) {
    setWindowTitle(windowTitle() + QStringLiteral(" - ") + Song::TextForFiletype(type));
    options_->layout()->setContentsMargins(0, 0, 0, 0);
    ui_->verticalLayout->insertWidget(0, options_);
    resize(width(), minimumHeight());
  }

}

TranscoderOptionsDialog::~TranscoderOptionsDialog() {
  delete ui_;
}

void TranscoderOptionsDialog::showEvent(QShowEvent *e) {

  Q_UNUSED(e);
  if (options_) {
    options_->Load();
  }

}

void TranscoderOptionsDialog::accept() {

  if (options_) {
    options_->Save();
  }
  QDialog::accept();

}

void TranscoderOptionsDialog::set_settings_postfix(const QString &settings_postfix) {

  if (options_) {
    options_->settings_postfix_ = settings_postfix;
  }

}
