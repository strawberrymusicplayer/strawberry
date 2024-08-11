/*
 * Strawberry Music Player
 * Copyright 2020-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include <QDialog>
#include <QString>
#include <QIcon>

class Ui_MessageDialog;

class MessageDialog : public QDialog {
  Q_OBJECT

 public:
  explicit MessageDialog(QWidget *parent = nullptr);
  ~MessageDialog() override;

  void set_settings_group(const QString &settings_group) { settings_group_ = settings_group; }
  void set_do_not_show_message_again(const QString &do_not_show_message_again) { do_not_show_message_again_ = do_not_show_message_again; }

  void ShowMessage(const QString &title, const QString &message, const QIcon &icon = QIcon());

 private Q_SLOTS:
  void DoNotShowMessageAgain();

 protected:
  Ui_MessageDialog *ui_;
  QWidget *parent_;
  QString settings_group_;
  QString do_not_show_message_again_;
};

#endif  // MESSAGEDIALOG_H
