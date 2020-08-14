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

#include <QWidget>
#include <QDialog>
#include <QApplication>
#include <QClipboard>
#include <QImage>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "core/network.h"
#include "widgets/busyindicator.h"
#include "coverfromurldialog.h"
#include "ui_coverfromurldialog.h"

CoverFromURLDialog::CoverFromURLDialog(QWidget *parent) : QDialog(parent), ui_(new Ui_CoverFromURLDialog), network_(new NetworkAccessManager(this)) {

  ui_->setupUi(this);
  ui_->busy->hide();

}

CoverFromURLDialog::~CoverFromURLDialog() {
  delete ui_;
}

QImage CoverFromURLDialog::Exec() {

  // reset state
  ui_->url->setText("");;
  last_image_ = QImage();

  QClipboard *clipboard = QApplication::clipboard();
  ui_->url->setText(clipboard->text());

  exec();
  return last_image_;

}

void CoverFromURLDialog::accept() {

  ui_->busy->show();

  QNetworkRequest req(QUrl::fromUserInput(ui_->url->text()));
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

  QNetworkReply *reply = network_->get(req);
  connect(reply, SIGNAL(finished()), SLOT(LoadCoverFromURLFinished()));

}

void CoverFromURLDialog::LoadCoverFromURLFinished() {

  ui_->busy->hide();

  QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    QMessageBox::information(this, tr("Fetching cover error"), tr("The site you requested does not exist!"));
    return;
  }

  QImage image;
  image.loadFromData(reply->readAll());

  if (!image.isNull()) {
    last_image_ = image;
    QDialog::accept();
  }
  else {
    QMessageBox::information(this, tr("Fetching cover error"), tr("The site you requested is not an image!"));
  }

}

