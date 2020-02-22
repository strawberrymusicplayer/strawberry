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
#include <QUrl>
#include <QSettings>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QEvent>
#include <qt5keychain/keychain.h>

#include "settingsdialog.h"
#include "subsonicsettingspage.h"
#include "ui_subsonicsettingspage.h"
#include "core/application.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "internet/internetservices.h"
#include "subsonic/subsonicservice.h"

const char *SubsonicSettingsPage::kSettingsGroup = "Subsonic";

SubsonicSettingsPage::SubsonicSettingsPage(SettingsDialog *parent)
    : SettingsPage(parent),
      ui_(new Ui::SubsonicSettingsPage),
      service_(dialog()->app()->internet_services()->Service<SubsonicService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("subsonic"));

  connect(ui_->button_test, SIGNAL(clicked()), SLOT(TestClicked()));

  connect(this, SIGNAL(Test(QUrl, const QString&, const QString&)), service_, SLOT(SendPing(QUrl, const QString&, const QString&)));

  connect(service_, SIGNAL(TestFailure(QString)), SLOT(TestFailure(QString)));
  connect(service_, SIGNAL(TestSuccess()), SLOT(TestSuccess()));

  dialog()->installEventFilter(this);

}

SubsonicSettingsPage::~SubsonicSettingsPage() { delete ui_; }

void SubsonicSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value("enabled", false).toBool());
  ui_->server_url->setText(s.value("url").toString());
  username_ = s.value("username").toString();
  ui_->username->setText(username_);
  if (s.contains("password")) {
    QByteArray password = s.value("password").toByteArray();
    if (password.isEmpty()) ui_->password->clear();
    else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));
  }
  ui_->checkbox_verify_certificate->setChecked(s.value("verifycertificate", false).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value("downloadalbumcovers", true).toBool());
  s.endGroup();

  QKeychain::ReadPasswordJob *passwd_job = new QKeychain::ReadPasswordJob("Strawberry" + QString(kSettingsGroup), this);
  passwd_job->setKey(ui_->username->text());
  passwd_job->connect(passwd_job, SIGNAL(finished(QKeychain::Job*)), this, SLOT(PasswordReadFinished(QKeychain::Job*)));
  passwd_job->start();

}

void SubsonicSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);

  s.setValue("enabled", ui_->enable->isChecked());
  s.setValue("url", QUrl(ui_->server_url->text()));
  s.setValue("username", ui_->username->text());
  s.setValue("verifycertificate", ui_->checkbox_verify_certificate->isChecked());
  s.setValue("downloadalbumcovers", ui_->checkbox_download_album_covers->isChecked());

  if (s.contains("password")) {
    s.remove("password");
  }

  s.endGroup();

  if (username_ != ui_->username->text()) {
    QKeychain::DeletePasswordJob *passwd_job = new QKeychain::DeletePasswordJob("Strawberry" + QString(kSettingsGroup), this);
    passwd_job->setKey(username_);
    passwd_job->start();
    username_ = ui_->username->text();
  }

  QKeychain::WritePasswordJob *passwd_job = new QKeychain::WritePasswordJob("Strawberry" + QString(kSettingsGroup), this);
  passwd_job->setKey(ui_->username->text());
  passwd_job->setTextData(ui_->password->text());
  passwd_job->connect(passwd_job, SIGNAL(finished(QKeychain::Job*)), this, SLOT(PasswordWriteFinished(QKeychain::Job*)));
  passwd_job->start();

  service_->ReloadSettings();

}

void SubsonicSettingsPage::PasswordReadFinished(QKeychain::Job *job) {

  QKeychain::ReadPasswordJob *passwd_job = qobject_cast<QKeychain::ReadPasswordJob*>(job);

  if (passwd_job->error()) {
    qLog(Debug) << passwd_job->errorString();
  }
  else {
    ui_->password->setText(passwd_job->textData());
  }

}

void SubsonicSettingsPage::PasswordWriteFinished(QKeychain::Job *job) {

  QKeychain::WritePasswordJob *passwd_job = qobject_cast<QKeychain::WritePasswordJob*>(job);

  if (passwd_job->error()) {
    qLog(Debug) << passwd_job->errorString();
  }

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

  emit Test(server_url, ui_->username->text(), ui_->password->text());
  ui_->button_test->setEnabled(false);

}

bool SubsonicSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_test->setEnabled(true);
    return false;
  }

  return SettingsPage::eventFilter(object, event);

}

void SubsonicSettingsPage::TestSuccess() {

  if (!this->isVisible()) return;
  ui_->button_test->setEnabled(true);

  QMessageBox::information(this, tr("Test successful!"), tr("Test successful!"));

}

void SubsonicSettingsPage::TestFailure(QString failure_reason) {

  if (!this->isVisible()) return;
  ui_->button_test->setEnabled(true);

  QMessageBox::warning(this, tr("Test failed!"), failure_reason);

}
