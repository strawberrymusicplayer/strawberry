/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QVariant>
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

#include "constants/dropboxsettings.h"
#include "core/settings.h"
#include "core/iconloader.h"
#include "widgets/loginstatewidget.h"
#include "dropbox/dropboxservice.h"
#include "settingsdialog.h"
#include "dropboxsettingspage.h"
#include "ui_dropboxsettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace DropboxSettings;

DropboxSettingsPage::DropboxSettingsPage(SettingsDialog *dialog, const SharedPtr<DropboxService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_DropboxSettingsPage),
      service_(service) {

  Q_ASSERT(service);

  ui_->setupUi(this);

  setWindowIcon(IconLoader::Load(u"dropbox"_s));

  ui_->login_state->AddCredentialGroup(ui_->widget_authorization);

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &DropboxSettingsPage::LoginClicked);
  QObject::connect(ui_->button_reset, &QPushButton::clicked, this, &DropboxSettingsPage::ResetClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &DropboxSettingsPage::LogoutClicked);

  QObject::connect(this, &DropboxSettingsPage::Authorize, &*service_, &DropboxService::Authenticate);
  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &DropboxSettingsPage::LoginFailure);
  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &DropboxSettingsPage::LoginSuccess);

  dialog->installEventFilter(this);

}

DropboxSettingsPage::~DropboxSettingsPage() {
  delete ui_;
}

void DropboxSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value(kEnabled, false).toBool());
  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_dropboxsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void DropboxSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->enable->isChecked());
  s.endGroup();

}

void DropboxSettingsPage::LoginClicked() {

  Q_EMIT Authorize();

  ui_->button_login->setEnabled(false);

}

bool DropboxSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void DropboxSettingsPage::LogoutClicked() {

  service_->ClearSession();
  ui_->button_login->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);

}

void DropboxSettingsPage::LoginSuccess() {

  if (!isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_login->setEnabled(true);

}

void DropboxSettingsPage::LoginFailure(const QString &failure_reason) {

  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
  ui_->button_login->setEnabled(true);

}

void DropboxSettingsPage::ResetClicked() {

  service_->Reset();

}
