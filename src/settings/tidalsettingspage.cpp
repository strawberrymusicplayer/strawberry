/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>
#include <QSettings>
#include <QMessageBox>
#include <QEvent>

#include "tidalsettingspage.h"
#include "ui_tidalsettingspage.h"
#include "core/application.h"
#include "core/iconloader.h"
#include "internet/internetservices.h"
#include "tidal/tidalservice.h"

const char *TidalSettingsPage::kSettingsGroup = "Tidal";

TidalSettingsPage::TidalSettingsPage(SettingsDialog *parent)
    : SettingsPage(parent),
      ui_(new Ui::TidalSettingsPage),
      service_(dialog()->app()->internet_services()->Service<TidalService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("tidal"));

  connect(ui_->button_login, SIGNAL(clicked()), SLOT(LoginClicked()));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(LogoutClicked()));

  connect(this, SIGNAL(Login(QString, QString, QString)), service_, SLOT(SendLogin(QString, QString, QString)));

  connect(service_, SIGNAL(LoginFailure(QString)), SLOT(LoginFailure(QString)));
  connect(service_, SIGNAL(LoginSuccess()), SLOT(LoginSuccess()));

  dialog()->installEventFilter(this);

  ui_->quality->addItem("Low", "LOW");
  ui_->quality->addItem("High", "HIGH");
  ui_->quality->addItem("Lossless", "LOSSLESS");
  ui_->quality->addItem("Hi resolution", "HI_RES");

  ui_->coversize->addItem("160x160", "160x160");
  ui_->coversize->addItem("320x320", "320x320");
  ui_->coversize->addItem("640x640", "640x640");
  ui_->coversize->addItem("750x750", "750x750");
  ui_->coversize->addItem("1280x1280", "1280x1280");

}

TidalSettingsPage::~TidalSettingsPage() { delete ui_; }

void TidalSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->checkbox_enable->setChecked(s.value("enabled", false).toBool());
  ui_->username->setText(s.value("username").toString());
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));
  ui_->token->setText(s.value("token").toString());
  dialog()->ComboBoxLoadFromSettings(s, ui_->quality, "quality", "HIGH");
  ui_->searchdelay->setValue(s.value("searchdelay", 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value("artistssearchlimit", 5).toInt());
  ui_->albumssearchlimit->setValue(s.value("albumssearchlimit", 100).toInt());
  ui_->songssearchlimit->setValue(s.value("songssearchlimit", 100).toInt());
  ui_->checkbox_fetchalbums->setChecked(s.value("fetchalbums", false).toBool());
  ui_->checkbox_cache_album_covers->setChecked(s.value("cachealbumcovers", true).toBool());
  dialog()->ComboBoxLoadFromSettings(s, ui_->coversize, "coversize", "320x320");
  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedIn);

}

void TidalSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->checkbox_enable->isChecked());
  s.setValue("username", ui_->username->text());
  s.setValue("password", QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));
  s.setValue("token", ui_->token->text());
  s.setValue("quality", ui_->quality->itemData(ui_->quality->currentIndex()));
  s.setValue("searchdelay", ui_->searchdelay->value());
  s.setValue("artistssearchlimit", ui_->artistssearchlimit->value());
  s.setValue("albumssearchlimit", ui_->albumssearchlimit->value());
  s.setValue("songssearchlimit", ui_->songssearchlimit->value());
  s.setValue("fetchalbums", ui_->checkbox_fetchalbums->isChecked());
  s.setValue("cachealbumcovers", ui_->checkbox_cache_album_covers->isChecked());
  s.setValue("coversize", ui_->coversize->itemData(ui_->coversize->currentIndex()));
  s.endGroup();

  service_->ReloadSettings();

}

void TidalSettingsPage::LoginClicked() {
  emit Login(ui_->username->text(), ui_->password->text(), ui_->token->text());
  ui_->button_login->setEnabled(false);
}

bool TidalSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
    return false;
  }

  return SettingsPage::eventFilter(object, event);
}

void TidalSettingsPage::LogoutClicked() {
  service_->Logout();
  ui_->button_login->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedOut);
}

void TidalSettingsPage::LoginSuccess() {
  if (!this->isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedIn);
  ui_->button_login->setEnabled(false);
}

void TidalSettingsPage::LoginFailure(QString failure_reason) {
  if (!this->isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
}
