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

#ifndef ADDSTREAMDIALOG_H
#define ADDSTREAMDIALOG_H

#include <QDialog>
#include <QString>
#include <QUrl>
#include <QLineEdit>

#include "ui_addstreamdialog.h"

class AddStreamDialog : public QDialog {
  Q_OBJECT

 public:
  explicit AddStreamDialog(QWidget *parent = nullptr);
  ~AddStreamDialog() override;

  QUrl url() const { return QUrl(ui_->url->text()); }
  void set_url(const QUrl &url) { ui_->url->setText(url.toString()); }

 protected:
  void showEvent(QShowEvent *e) override;

 private Q_SLOTS:
  void TextChanged(const QString &text);

 private:
  Ui_AddStreamDialog *ui_;
};

#endif  // ADDSTREAMDIALOG_H
