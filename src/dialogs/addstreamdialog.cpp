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

#include "addstreamdialog.h"
#include "ui_addstreamdialog.h"

#include <QUrl>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QShowEvent>

AddStreamDialog::AddStreamDialog(QWidget *parent) : QDialog(parent), ui_(new Ui_AddStreamDialog) {

  ui_->setupUi(this);

  QObject::connect(ui_->url, &QLineEdit::textChanged, this, &AddStreamDialog::TextChanged);
  TextChanged(QString());

}

AddStreamDialog::~AddStreamDialog() { delete ui_; }

void AddStreamDialog::showEvent(QShowEvent *e) {

  if (!e->spontaneous()) {
    ui_->url->setFocus();
    ui_->url->selectAll();
  }

  QDialog::showEvent(e);

}

void AddStreamDialog::TextChanged(const QString &text) {

  QUrl url(text);
  ui_->button_box->button(QDialogButtonBox::Ok)->setEnabled(url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty());

}
