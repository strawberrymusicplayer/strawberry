/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QUrl>
#include <QSettings>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QEvent>

#include "settingsdialog.h"
#include "subsonicsettingspage.h"
#include "ui_subsonicsettingspage.h"
#include "core/application.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "streaming/streamingservices.h"
#include "subsonic/subsonicservice.h"

const char *SubsonicSettingsPage::kSettingsGroup = "Subsonic";

SubsonicSettingsPage::SubsonicSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::SubsonicSettingsPage),
      service_(dialog->app()->streaming_services()->Service<SubsonicService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(QStringLiteral("subsonic"), true, 0, 32));

  QObject::connect(ui_->button_test, &QPushButton::clicked, this, &SubsonicSettingsPage::TestClicked);
  QObject::connect(ui_->button_deletesongs, &QPushButton::clicked, &*service_, &SubsonicService::DeleteSongs);

  QObject::connect(this, &SubsonicSettingsPage::Test, &*service_, &SubsonicService::SendPingWithCredentials);

  QObject::connect(&*service_, &SubsonicService::TestFailure, this, &SubsonicSettingsPage::TestFailure);
  QObject::connect(&*service_, &SubsonicService::TestSuccess, this, &SubsonicSettingsPage::TestSuccess);

  dialog->installEventFilter(this);

  ui_->checkbox_http2->show();

}

SubsonicSettingsPage::~SubsonicSettingsPage() { delete ui_; }

void SubsonicSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value("enabled", false).toBool());
  ui_->server_url->setText(s.value("url").toString());
  ui_->username->setText(s.value("username").toString());
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));
  ui_->checkbox_http2->setChecked(s.value("http2", false).toBool());
  ui_->checkbox_verify_certificate->setChecked(s.value("verifycertificate", false).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value("downloadalbumcovers", true).toBool());
  ui_->checkbox_server_scrobbling->setChecked(s.value("serversidescrobbling", false).toBool());

  const AuthMethod auth_method = static_cast<AuthMethod>(s.value("authmethod", static_cast<int>(AuthMethod::MD5)).toInt());
  switch (auth_method) {
    case AuthMethod::Hex:
      ui_->auth_method_hex->setChecked(true);
      break;
    case AuthMethod::MD5:
      ui_->auth_method_md5->setChecked(true);
      break;
  }

  s.endGroup();

  Init(ui_->layout_subsonicsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void SubsonicSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->enable->isChecked());
  s.setValue("url", QUrl(ui_->server_url->text()));
  s.setValue("username", ui_->username->text());
  s.setValue("password", QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));
  s.setValue("http2", ui_->checkbox_http2->isChecked());
  s.setValue("verifycertificate", ui_->checkbox_verify_certificate->isChecked());
  s.setValue("downloadalbumcovers", ui_->checkbox_download_album_covers->isChecked());
  s.setValue("serversidescrobbling", ui_->checkbox_server_scrobbling->isChecked());
  if (ui_->auth_method_hex->isChecked()) {
    s.setValue("authmethod", static_cast<int>(AuthMethod::Hex));
  }
  else {
    s.setValue("authmethod", static_cast<int>(AuthMethod::MD5));
  }
  s.endGroup();

}

void SubsonicSettingsPage::TestClicked() {

  if (ui_->server_url->text().isEmpty() || ui_->username->text().isEmpty() || ui_->password->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing server url, username or password."));
    return;
  }

  QUrl server_url(ui_->server_url->text());
  if (!server_url.isValid() || server_url.scheme().isEmpty() || server_url.host().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incorrect"), tr("Server URL is invalid."));
    return;
  }

  Q_EMIT Test(server_url, ui_->username->text(), ui_->password->text(), ui_->auth_method_hex->isChecked() ? AuthMethod::Hex : AuthMethod::MD5);
  ui_->button_test->setEnabled(false);

}

bool SubsonicSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_test->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void SubsonicSettingsPage::TestSuccess() {

  if (!isVisible()) return;
  ui_->button_test->setEnabled(true);

  QMessageBox::information(this, tr("Test successful!"), tr("Test successful!"));

}

void SubsonicSettingsPage::TestFailure(const QString &failure_reason) {

  if (!isVisible()) return;
  ui_->button_test->setEnabled(true);

  QMessageBox::warning(this, tr("Test failed!"), failure_reason);

}
