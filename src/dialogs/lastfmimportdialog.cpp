/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QDialog>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QCloseEvent>

#include "lastfmimportdialog.h"
#include "ui_lastfmimportdialog.h"

#include "includes/shared_ptr.h"
#include "core/iconloader.h"
#include "scrobbler/lastfmimport.h"

using namespace Qt::Literals::StringLiterals;

LastFMImportDialog::LastFMImportDialog(SharedPtr<LastFMImport> lastfm_import, QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_LastFMImportDialog),
      lastfm_import_(lastfm_import),
      finished_(false),
      playcount_total_(0),
      lastplayed_total_(0) {

  ui_->setupUi(this);

  setWindowIcon(IconLoader::Load(u"scrobble"_s));

  ui_->stackedWidget->setCurrentWidget(ui_->page_start);

  Reset();

  QObject::connect(ui_->button_close, &QPushButton::clicked, this, &LastFMImportDialog::Close);
  QObject::connect(ui_->button_go, &QPushButton::clicked, this, &LastFMImportDialog::Start);
  QObject::connect(ui_->button_cancel, &QPushButton::clicked, this, &LastFMImportDialog::Cancel);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  QObject::connect(ui_->checkbox_last_played, &QCheckBox::checkStateChanged, this, &LastFMImportDialog::UpdateGoButtonState);
  QObject::connect(ui_->checkbox_playcounts, &QCheckBox::checkStateChanged, this, &LastFMImportDialog::UpdateGoButtonState);
#else
  QObject::connect(ui_->checkbox_last_played, &QCheckBox::stateChanged, this, &LastFMImportDialog::UpdateGoButtonState);
  QObject::connect(ui_->checkbox_playcounts, &QCheckBox::stateChanged, this, &LastFMImportDialog::UpdateGoButtonState);
#endif

}

LastFMImportDialog::~LastFMImportDialog() { delete ui_; }

void LastFMImportDialog::closeEvent(QCloseEvent *e) {

  ResetFinished();

  QDialog::closeEvent(e);

}

void LastFMImportDialog::Start() {

  if (ui_->stackedWidget->currentWidget() == ui_->page_start && (ui_->checkbox_last_played->isChecked() || ui_->checkbox_playcounts->isChecked())) {
    ui_->stackedWidget->setCurrentWidget(ui_->page_progress);
    ui_->button_go->hide();
    ui_->button_cancel->show();
    ui_->label_progress_top->setText(tr("Receiving initial data from last.fm..."));
    lastfm_import_->ImportData(ui_->checkbox_last_played->isChecked(), ui_->checkbox_playcounts->isChecked());
  }

}

void LastFMImportDialog::Cancel() {

  if (ui_->stackedWidget->currentWidget() == ui_->page_progress) {
    lastfm_import_->AbortAll();
    ui_->stackedWidget->setCurrentWidget(ui_->page_start);
    Reset();
  }

}

void LastFMImportDialog::Close() {

  ResetFinished();
  hide();

}

void LastFMImportDialog::ResetFinished() {

  if (finished_ && ui_->stackedWidget->currentWidget() == ui_->page_progress) {
    finished_ = false;
    Reset();
    ui_->stackedWidget->setCurrentWidget(ui_->page_start);
  }

}

void LastFMImportDialog::Reset() {

  ui_->button_go->show();
  ui_->button_cancel->hide();

  playcount_total_ = 0;
  lastplayed_total_ = 0;

  ui_->progressbar->setValue(0);
  ui_->label_progress_top->clear();
  ui_->label_progress_bottom->clear();

  UpdateGoButtonState();

}

void LastFMImportDialog::UpdateTotal(const int lastplayed_total, const int playcount_total) {

  if (ui_->stackedWidget->currentWidget() != ui_->page_progress) return;

  playcount_total_ = playcount_total;
  lastplayed_total_ = lastplayed_total;

  if (lastplayed_total > 0 && playcount_total > 0) {
    ui_->label_progress_top->setText(tr("Receiving playcount for %1 songs and last played for %2 songs.").arg(playcount_total).arg(lastplayed_total));
  }
  else if (lastplayed_total > 0) {
    ui_->label_progress_top->setText(tr("Receiving last played for %1 songs.").arg(lastplayed_total));
  }
  else if (playcount_total > 0) {
    ui_->label_progress_top->setText(tr("Receiving playcounts for %1 songs.").arg(playcount_total));
  }
  else {
    ui_->label_progress_top->clear();
  }

  ui_->label_progress_bottom->clear();

}

void LastFMImportDialog::UpdateProgress(const int lastplayed_received, const int playcount_received) {

  if (ui_->stackedWidget->currentWidget() != ui_->page_progress) return;

  ui_->progressbar->setValue(static_cast<int>(static_cast<float>(playcount_received + lastplayed_received) / static_cast<float>(playcount_total_ + lastplayed_total_) * 100.0));

  if (lastplayed_received > 0 && playcount_received > 0) {
    ui_->label_progress_bottom->setText(tr("Playcounts for %1 songs and last played for %2 songs received.").arg(playcount_received).arg(lastplayed_received));
  }
  else if (lastplayed_received > 0) {
    ui_->label_progress_bottom->setText(tr("Last played for %1 songs received.").arg(lastplayed_received));
  }
  else if (playcount_received > 0) {
    ui_->label_progress_bottom->setText(tr("Playcounts for %1 songs received.").arg(playcount_received));
  }
  else {
    ui_->label_progress_bottom->clear();
  }

}

void LastFMImportDialog::Finished() {

  ui_->button_cancel->hide();
  finished_ = true;

}

void LastFMImportDialog::FinishedWithError(const QString &error) {

  Finished();
  ui_->label_progress_bottom->setText(error);

}

void LastFMImportDialog::UpdateGoButtonState() {
  ui_->button_go->setEnabled(ui_->checkbox_last_played->isChecked() || ui_->checkbox_playcounts->isChecked());
}
