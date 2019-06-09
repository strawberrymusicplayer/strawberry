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
#include "tidal/tidalstreamurlrequest.h"

const char *TidalSettingsPage::kSettingsGroup = "Tidal";

TidalSettingsPage::TidalSettingsPage(SettingsDialog *parent)
    : SettingsPage(parent),
      ui_(new Ui::TidalSettingsPage),
      service_(dialog()->app()->internet_services()->Service<TidalService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("tidal"));

  connect(ui_->button_login, SIGNAL(clicked()), SLOT(LoginClicked()));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(LogoutClicked()));
  connect(ui_->oauth, SIGNAL(toggled(bool)), SLOT(OAuthClicked(bool)));

  connect(this, SIGNAL(Login()), service_, SLOT(StartAuthorisation()));
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

  ui_->streamurl->addItem("streamurl", StreamUrlMethod_StreamUrl);
  ui_->streamurl->addItem("urlpostpaywall", StreamUrlMethod_UrlPostPaywall);
  ui_->streamurl->addItem("playbackinfopostpaywall", StreamUrlMethod_PlaybackInfoPostPaywall);

}

TidalSettingsPage::~TidalSettingsPage() { delete ui_; }

void TidalSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value("enabled", false).toBool());
  ui_->oauth->setChecked(s.value("oauth", false).toBool());

  ui_->client_id->setText(s.value("client_id").toString());
  ui_->api_token->setText(s.value("api_token").toString());
  ui_->user_id->setText(s.value("user_id").toString());
  ui_->country_code->setText(s.value("country_code").toString());

  ui_->username->setText(s.value("username").toString());
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));

  dialog()->ComboBoxLoadFromSettings(s, ui_->quality, "quality", "HIGH");
  ui_->searchdelay->setValue(s.value("searchdelay", 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value("artistssearchlimit", 5).toInt());
  ui_->albumssearchlimit->setValue(s.value("albumssearchlimit", 100).toInt());
  ui_->songssearchlimit->setValue(s.value("songssearchlimit", 100).toInt());
  ui_->checkbox_fetchalbums->setChecked(s.value("fetchalbums", false).toBool());
  ui_->checkbox_cache_album_covers->setChecked(s.value("cachealbumcovers", true).toBool());
  dialog()->ComboBoxLoadFromSettings(s, ui_->coversize, "coversize", "320x320");

  StreamUrlMethod stream_url = static_cast<StreamUrlMethod>(s.value("streamurl").toInt());
  int i = ui_->streamurl->findData(stream_url);
  if (i == -1) i = ui_->streamurl->findData(StreamUrlMethod_StreamUrl);
  ui_->streamurl->setCurrentIndex(i);

  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedIn);

}

void TidalSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->enable->isChecked());
  s.setValue("oauth", ui_->oauth->isChecked());
  s.setValue("client_id", ui_->client_id->text());
  s.setValue("api_token", ui_->api_token->text());
  s.setValue("user_id", ui_->user_id->text());
  s.setValue("country_code", ui_->country_code->text());

  s.setValue("username", ui_->username->text());
  s.setValue("password", QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));

  s.setValue("quality", ui_->quality->itemData(ui_->quality->currentIndex()));
  s.setValue("searchdelay", ui_->searchdelay->value());
  s.setValue("artistssearchlimit", ui_->artistssearchlimit->value());
  s.setValue("albumssearchlimit", ui_->albumssearchlimit->value());
  s.setValue("songssearchlimit", ui_->songssearchlimit->value());
  s.setValue("fetchalbums", ui_->checkbox_fetchalbums->isChecked());
  s.setValue("cachealbumcovers", ui_->checkbox_cache_album_covers->isChecked());
  s.setValue("coversize", ui_->coversize->itemData(ui_->coversize->currentIndex()));
  s.setValue("streamurl", ui_->streamurl->itemData(ui_->streamurl->currentIndex()));
  s.endGroup();

  service_->ReloadSettings();

}

void TidalSettingsPage::LoginClicked() {

  if (ui_->oauth->isChecked()) {
    emit Login();
  }
  else {
    if (ui_->username->text().isEmpty() || ui_->password->text().isEmpty()) {
      QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing username or password."));
      return;
    }
    emit Login(ui_->username->text(), ui_->password->text(), ui_->api_token->text());
  }
  ui_->button_login->setEnabled(false);

}

bool TidalSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
    return false;
  }

  return SettingsPage::eventFilter(object, event);

}

void TidalSettingsPage::OAuthClicked(bool enabled) {

  ui_->client_id->setEnabled(enabled);
  ui_->api_token->setEnabled(!enabled);
  ui_->username->setEnabled(!enabled);
  ui_->password->setEnabled(!enabled);

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
