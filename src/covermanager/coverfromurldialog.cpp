/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "config.h"

#include <QWidget>
#include <QDialog>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "utilities/mimeutils.h"
#include "widgets/busyindicator.h"
#include "albumcoverimageresult.h"
#include "coverfromurldialog.h"
#include "ui_coverfromurldialog.h"

using namespace Qt::Literals::StringLiterals;

CoverFromURLDialog::CoverFromURLDialog(SharedPtr<NetworkAccessManager> network, QWidget *parent)
    : QDialog(parent),
      network_(network),
      ui_(new Ui_CoverFromURLDialog) {

  ui_->setupUi(this);
  ui_->busy->hide();

}

CoverFromURLDialog::~CoverFromURLDialog() {
  delete ui_;
}

AlbumCoverImageResult CoverFromURLDialog::Exec() {

  // reset state
  ui_->url->setText(""_L1);
  last_album_cover_ = AlbumCoverImageResult();

  QClipboard *clipboard = QApplication::clipboard();
  ui_->url->setText(clipboard->text());

  exec();
  return last_album_cover_;

}

void CoverFromURLDialog::accept() {

  ui_->busy->show();

  QNetworkRequest network_request(QUrl::fromUserInput(ui_->url->text()));
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::finished, this, &CoverFromURLDialog::LoadCoverFromURLFinished);

}

void CoverFromURLDialog::LoadCoverFromURLFinished() {

  ui_->busy->hide();

  QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    QMessageBox::information(this, tr("Fetching cover error"), tr("The site you requested does not exist!"));
    return;
  }

  AlbumCoverImageResult result;
  result.image_data = reply->readAll();
  result.image.loadFromData(result.image_data);
  result.mime_type = Utilities::MimeTypeFromData(result.image_data);

  if (result.image.isNull()) {
    QMessageBox::information(this, tr("Fetching cover error"), tr("The site you requested is not an image!"));
  }
  else {
    last_album_cover_ = result;
    QDialog::accept();
  }

}
