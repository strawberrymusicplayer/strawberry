/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QString>
#include <QSettings>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QMessageBox>
#include <QEvent>

#include "settingsdialog.h"
#include "tidalsettingspage.h"
#include "ui_tidalsettingspage.h"
#include "core/application.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "streaming/streamingservices.h"
#include "tidal/tidalservice.h"
#include "widgets/loginstatewidget.h"

const char *TidalSettingsPage::kSettingsGroup = "Tidal";

TidalSettingsPage::TidalSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::TidalSettingsPage),
      service_(dialog->app()->streaming_services()->Service<TidalService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(QStringLiteral("tidal"), true, 0, 32));

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &TidalSettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &TidalSettingsPage::LogoutClicked);
  QObject::connect(ui_->oauth, &QCheckBox::toggled, this, &TidalSettingsPage::OAuthClicked);

  QObject::connect(this, &TidalSettingsPage::Authorize, &*service_, &TidalService::StartAuthorization);
  QObject::connect(this, &TidalSettingsPage::Login, &*service_, &TidalService::SendLoginWithCredentials);

  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &TidalSettingsPage::LoginFailure);
  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &TidalSettingsPage::LoginSuccess);

  dialog->installEventFilter(this);

  ui_->quality->addItem(QStringLiteral("Low"), QStringLiteral("LOW"));
  ui_->quality->addItem(QStringLiteral("High"), QStringLiteral("HIGH"));
  ui_->quality->addItem(QStringLiteral("Lossless"), QStringLiteral("LOSSLESS"));
  ui_->quality->addItem(QStringLiteral("Hi resolution"), QStringLiteral("HI_RES"));

  ui_->coversize->addItem(QStringLiteral("160x160"), QStringLiteral("160x160"));
  ui_->coversize->addItem(QStringLiteral("320x320"), QStringLiteral("320x320"));
  ui_->coversize->addItem(QStringLiteral("640x640"), QStringLiteral("640x640"));
  ui_->coversize->addItem(QStringLiteral("750x750"), QStringLiteral("750x750"));
  ui_->coversize->addItem(QStringLiteral("1280x1280"), QStringLiteral("1280x1280"));

  ui_->streamurl->addItem(QStringLiteral("streamurl"), static_cast<int>(StreamUrlMethod::StreamUrl));
  ui_->streamurl->addItem(QStringLiteral("urlpostpaywall"), static_cast<int>(StreamUrlMethod::UrlPostPaywall));
  ui_->streamurl->addItem(QStringLiteral("playbackinfopostpaywall"), static_cast<int>(StreamUrlMethod::PlaybackInfoPostPaywall));

}

TidalSettingsPage::~TidalSettingsPage() { delete ui_; }

void TidalSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value("enabled", false).toBool());
  ui_->oauth->setChecked(s.value("oauth", true).toBool());

  ui_->client_id->setText(s.value("client_id").toString());
  ui_->api_token->setText(s.value("api_token").toString());

  ui_->username->setText(s.value("username").toString());
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));

  ComboBoxLoadFromSettings(s, ui_->quality, QStringLiteral("quality"), QStringLiteral("LOSSLESS"));
  ui_->searchdelay->setValue(s.value("searchdelay", 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value("artistssearchlimit", 4).toInt());
  ui_->albumssearchlimit->setValue(s.value("albumssearchlimit", 10).toInt());
  ui_->songssearchlimit->setValue(s.value("songssearchlimit", 10).toInt());
  ui_->checkbox_fetchalbums->setChecked(s.value("fetchalbums", false).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value("downloadalbumcovers", true).toBool());
  ComboBoxLoadFromSettings(s, ui_->coversize, QStringLiteral("coversize"), QStringLiteral("640x640"));
  ui_->streamurl->setCurrentIndex(ui_->streamurl->findData(s.value("streamurl", static_cast<int>(StreamUrlMethod::StreamUrl)).toInt()));
  ui_->checkbox_album_explicit->setChecked(s.value("album_explicit", false).toBool());

  s.endGroup();

  OAuthClicked(ui_->oauth->isChecked());
  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_tidalsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void TidalSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->enable->isChecked());
  s.setValue("oauth", ui_->oauth->isChecked());
  s.setValue("client_id", ui_->client_id->text());
  s.setValue("api_token", ui_->api_token->text());

  s.setValue("username", ui_->username->text());
  s.setValue("password", QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));

  s.setValue("quality", ui_->quality->currentData().toString());
  s.setValue("searchdelay", ui_->searchdelay->value());
  s.setValue("artistssearchlimit", ui_->artistssearchlimit->value());
  s.setValue("albumssearchlimit", ui_->albumssearchlimit->value());
  s.setValue("songssearchlimit", ui_->songssearchlimit->value());
  s.setValue("fetchalbums", ui_->checkbox_fetchalbums->isChecked());
  s.setValue("downloadalbumcovers", ui_->checkbox_download_album_covers->isChecked());
  s.setValue("coversize", ui_->coversize->currentData().toString());
  s.setValue("streamurl", ui_->streamurl->currentData().toInt());
  s.setValue("album_explicit", ui_->checkbox_album_explicit->isChecked());
  s.endGroup();

}

void TidalSettingsPage::LoginClicked() {

  if (ui_->oauth->isChecked()) {
    if (ui_->client_id->text().isEmpty()) {
      QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing Tidal client ID."));
      return;
    }
    Q_EMIT Authorize(ui_->client_id->text());
  }
  else {
    if (ui_->api_token->text().isEmpty()) {
      QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing API token."));
      return;
    }
    if (ui_->username->text().isEmpty()) {
      QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing username."));
      return;
    }
    if (ui_->password->text().isEmpty()) {
      QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing password."));
      return;
    }
    Q_EMIT Login(ui_->api_token->text(), ui_->username->text(), ui_->password->text());
  }
  ui_->button_login->setEnabled(false);

}

bool TidalSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void TidalSettingsPage::OAuthClicked(const bool enabled) {

  ui_->client_id->setEnabled(enabled);
  ui_->api_token->setEnabled(!enabled);
  ui_->username->setEnabled(!enabled);
  ui_->password->setEnabled(!enabled);

}

void TidalSettingsPage::LogoutClicked() {

  service_->Logout();
  ui_->button_login->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);

}

void TidalSettingsPage::LoginSuccess() {

  if (!isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_login->setEnabled(true);

}

void TidalSettingsPage::LoginFailure(const QString &failure_reason) {

  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
  ui_->button_login->setEnabled(true);

}
