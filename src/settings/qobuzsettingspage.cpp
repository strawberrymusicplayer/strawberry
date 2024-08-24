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
#include "core/settings.h"
#include "widgets/loginstatewidget.h"
#include "streaming/streamingservices.h"
#include "qobuz/qobuzservice.h"

const char *QobuzSettingsPage::kSettingsGroup = "Qobuz";

QobuzSettingsPage::QobuzSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::QobuzSettingsPage),
      service_(dialog->app()->streaming_services()->Service<QobuzService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(QStringLiteral("qobuz"), true, 0, 32));

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &QobuzSettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &QobuzSettingsPage::LogoutClicked);

  QObject::connect(this, &QobuzSettingsPage::Login, &*service_, &StreamingService::LoginWithCredentials);

  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &QobuzSettingsPage::LoginFailure);
  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &QobuzSettingsPage::LoginSuccess);

  dialog->installEventFilter(this);

  ui_->format->addItem(QStringLiteral("MP3 320"), 5);
  ui_->format->addItem(QStringLiteral("FLAC Lossless"), 6);
  ui_->format->addItem(QStringLiteral("FLAC Hi-Res <= 96kHz"), 7);
  ui_->format->addItem(QStringLiteral("FLAC Hi-Res > 96kHz"), 27);

}

QobuzSettingsPage::~QobuzSettingsPage() { delete ui_; }

void QobuzSettingsPage::Load() {

  Settings s;
  if (!s.contains(kSettingsGroup)) set_changed();

  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value("enabled", false).toBool());
  ui_->app_id->setText(s.value("app_id").toString());
  ui_->app_secret->setText(s.value("app_secret").toString());

  ui_->username->setText(s.value("username").toString());
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));

  ComboBoxLoadFromSettings(s, ui_->format, QStringLiteral("format"), 27);
  ui_->searchdelay->setValue(s.value("searchdelay", 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value("artistssearchlimit", 4).toInt());
  ui_->albumssearchlimit->setValue(s.value("albumssearchlimit", 10).toInt());
  ui_->songssearchlimit->setValue(s.value("songssearchlimit", 10).toInt());
  ui_->checkbox_base64_secret->setChecked(s.value("base64secret", false).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value("downloadalbumcovers", true).toBool());

  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_qobuzsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void QobuzSettingsPage::Save() {

  Settings s;
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
  s.setValue("base64secret", ui_->checkbox_base64_secret->isChecked());
  s.setValue("downloadalbumcovers", ui_->checkbox_download_album_covers->isChecked());
  s.endGroup();

}

void QobuzSettingsPage::LoginClicked() {

  if (ui_->app_id->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing app id."));
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

  Q_EMIT Login(ui_->app_id->text(), ui_->username->text(), ui_->password->text());
  ui_->button_login->setEnabled(false);

}

bool QobuzSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void QobuzSettingsPage::LogoutClicked() {

  service_->Logout();
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
  ui_->button_login->setEnabled(true);

}

void QobuzSettingsPage::LoginSuccess() {

  if (!isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_login->setEnabled(true);

}

void QobuzSettingsPage::LoginFailure(const QString &failure_reason) {

  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);

}
