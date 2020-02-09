/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QSettings>
#include <QMessageBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QEvent>

#include "settingsdialog.h"
#include "qobuzsettingspage.h"
#include "ui_qobuzsettingspage.h"
#include "core/application.h"
#include "core/iconloader.h"
#include "widgets/loginstatewidget.h"
#include "internet/internetservices.h"
#include "qobuz/qobuzservice.h"

const char *QobuzSettingsPage::kSettingsGroup = "Qobuz";

QobuzSettingsPage::QobuzSettingsPage(SettingsDialog *parent)
    : SettingsPage(parent),
      ui_(new Ui::QobuzSettingsPage),
      service_(dialog()->app()->internet_services()->Service<QobuzService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("qobuz"));

  connect(ui_->button_login, SIGNAL(clicked()), SLOT(LoginClicked()));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(LogoutClicked()));

  connect(this, SIGNAL(Login(QString, QString, QString)), service_, SLOT(SendLogin(QString, QString, QString)));

  connect(service_, SIGNAL(LoginFailure(QString)), SLOT(LoginFailure(QString)));
  connect(service_, SIGNAL(LoginSuccess()), SLOT(LoginSuccess()));

  dialog()->installEventFilter(this);

  ui_->format->addItem("MP3 320", 5);
  ui_->format->addItem("FLAC Lossless", 6);
  ui_->format->addItem("FLAC Hi-Res <= 96kHz", 7);
  ui_->format->addItem("FLAC Hi-Res > 96kHz", 27);

}

QobuzSettingsPage::~QobuzSettingsPage() { delete ui_; }

void QobuzSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value("enabled", false).toBool());
  ui_->app_id->setText(s.value("app_id").toString());
  ui_->app_secret->setText(s.value("app_secret").toString());

  ui_->username->setText(s.value("username").toString());
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));

  dialog()->ComboBoxLoadFromSettings(s, ui_->format, "format", 27);
  ui_->searchdelay->setValue(s.value("searchdelay", 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value("artistssearchlimit", 4).toInt());
  ui_->albumssearchlimit->setValue(s.value("albumssearchlimit", 10).toInt());
  ui_->songssearchlimit->setValue(s.value("songssearchlimit", 10).toInt());
  ui_->checkbox_download_album_covers->setChecked(s.value("downloadalbumcovers", true).toBool());

  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedIn);

}

void QobuzSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->enable->isChecked());
  s.setValue("app_id", ui_->app_id->text());
  s.setValue("app_secret", ui_->app_secret->text());

  s.setValue("username", ui_->username->text());
  s.setValue("password", QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));

  s.setValue("format", ui_->format->itemData(ui_->format->currentIndex()));
  s.setValue("searchdelay", ui_->searchdelay->value());
  s.setValue("artistssearchlimit", ui_->artistssearchlimit->value());
  s.setValue("albumssearchlimit", ui_->albumssearchlimit->value());
  s.setValue("songssearchlimit", ui_->songssearchlimit->value());
  s.setValue("downloadalbumcovers", ui_->checkbox_download_album_covers->isChecked());
  s.endGroup();

  service_->ReloadSettings();

}

void QobuzSettingsPage::LoginClicked() {

  if (ui_->app_id->text().isEmpty() || ui_->username->text().isEmpty() || ui_->password->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing app id, username or password."));
    return;
  }
  emit Login(ui_->app_id->text(), ui_->username->text(), ui_->password->text());
  ui_->button_login->setEnabled(false);

}

bool QobuzSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
    return false;
  }

  return SettingsPage::eventFilter(object, event);

}

void QobuzSettingsPage::LogoutClicked() {
  service_->Logout();
  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedOut);
  ui_->button_login->setEnabled(true);
}

void QobuzSettingsPage::LoginSuccess() {
  if (!this->isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedIn);
  ui_->button_login->setEnabled(true);
}

void QobuzSettingsPage::LoginFailure(QString failure_reason) {
  if (!this->isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
}
