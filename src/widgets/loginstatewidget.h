/*
   This file was part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>
   Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef LOGINSTATEWIDGET_H
#define LOGINSTATEWIDGET_H

#include <QWidget>
#include <QObject>
#include <QList>
#include <QString>
#include <QDate>

class QEvent;

class Ui_LoginStateWidget;

class LoginStateWidget : public QWidget {
  Q_OBJECT

 public:
  explicit LoginStateWidget(QWidget *parent = nullptr);
  ~LoginStateWidget() override;

  enum class State {
    LoggedIn,
    LoginInProgress,
    LoggedOut
  };

  // Installs an event handler on the field so that pressing enter will emit
  // LoginClicked() instead of doing the default action (closing the dialog).
  void AddCredentialField(QWidget *widget);

  // This widget (usually a QGroupBox) will be hidden when SetLoggedIn(true) is called.
  void AddCredentialGroup(QWidget *widget);

  // QObject
  bool eventFilter(QObject *object, QEvent *event) override;

 public Q_SLOTS:
  // Changes the "You are logged in/out" label, shows/hides any QGroupBoxes added with AddCredentialGroup.
  void SetLoggedIn(const LoginStateWidget::State state, const QString &account_name = QString());

  // Hides the "You are logged in/out" label completely.
  void HideLoggedInState();

  void SetAccountTypeText(const QString &text);
  void SetAccountTypeVisible(const bool visible);

  void SetExpires(const QDate expires);

 Q_SIGNALS:
  void LogoutClicked();
  void LoginClicked();

 private Q_SLOTS:
  void Logout();
  void FocusLastCredentialField();

 private:
  Ui_LoginStateWidget *ui_;

  State state_;

  QList<QObject*> credential_fields_;
  QList<QWidget*> credential_groups_;
};

#endif  // LOGINSTATEWIDGET_H
