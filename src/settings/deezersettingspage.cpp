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

#include "deezersettingspage.h"
#include "ui_deezersettingspage.h"
#include "core/application.h"
#include "core/iconloader.h"
#include "internet/internetmodel.h"
#include "deezer/deezerservice.h"

const char *DeezerSettingsPage::kSettingsGroup = "Deezer";

DeezerSettingsPage::DeezerSettingsPage(SettingsDialog *parent)
    : SettingsPage(parent),
      ui_(new Ui::DeezerSettingsPage),
      service_(dialog()->app()->internet_model()->Service<DeezerService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("deezer"));

  connect(ui_->button_login, SIGNAL(clicked()), SLOT(LoginClicked()));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(LogoutClicked()));

  connect(this, SIGNAL(Login()), service_, SLOT(StartAuthorisation()));

  connect(service_, SIGNAL(LoginFailure(QString)), SLOT(LoginFailure(QString)));
  connect(service_, SIGNAL(LoginSuccess()), SLOT(LoginSuccess()));

  dialog()->installEventFilter(this);

  ui_->combobox_quality->addItem("AAC (64)", "AAC_64");
  ui_->combobox_quality->addItem("MP3 (64)", "MP3_64");
  ui_->combobox_quality->addItem("MP3 (128)", "MP3_128");
  ui_->combobox_quality->addItem("MP3 (256)", "MP3_256");
  ui_->combobox_quality->addItem("MP3 (320)", "MP3_320");
  ui_->combobox_quality->addItem("FLAC", "FLAC");

  ui_->combobox_coversize->addItem("Small", "cover_small");
  ui_->combobox_coversize->addItem("Medium", "cover_medium");
  ui_->combobox_coversize->addItem("Big", "cover_big");
  ui_->combobox_coversize->addItem("XL", "cover_xl");

}

DeezerSettingsPage::~DeezerSettingsPage() { delete ui_; }

void DeezerSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  ui_->username->setText(s.value("username").toString());
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));
  dialog()->ComboBoxLoadFromSettings(s, ui_->combobox_quality, "quality", "FLAC");
  ui_->spinbox_searchdelay->setValue(s.value("searchdelay", 1500).toInt());
  ui_->spinbox_albumssearchlimit->setValue(s.value("albumssearchlimit", 100).toInt());
  ui_->spinbox_songssearchlimit->setValue(s.value("songssearchlimit", 100).toInt());
  ui_->checkbox_fetchalbums->setChecked(s.value("fetchalbums", false).toBool());
  dialog()->ComboBoxLoadFromSettings(s, ui_->combobox_coversize, "coversize", "cover_big");
  ui_->checkbox_preview->setChecked(s.value("preview", false).toBool());
  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedIn);

}

void DeezerSettingsPage::Save() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("username", ui_->username->text());
  s.setValue("password", QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));
  s.setValue("quality", ui_->combobox_quality->itemData(ui_->combobox_quality->currentIndex()));
  s.setValue("searchdelay", ui_->spinbox_searchdelay->value());
  s.setValue("albumssearchlimit", ui_->spinbox_albumssearchlimit->value());
  s.setValue("songssearchlimit", ui_->spinbox_songssearchlimit->value());
  s.setValue("fetchalbums", ui_->checkbox_fetchalbums->isChecked());
  s.setValue("coversize", ui_->combobox_coversize->itemData(ui_->combobox_coversize->currentIndex()));
  s.setValue("preview", ui_->checkbox_preview->isChecked());
  s.endGroup();

  service_->ReloadSettings();

}

void DeezerSettingsPage::LoginClicked() {
  emit Login();
  ui_->button_login->setEnabled(false);
}

bool DeezerSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
    return false;
  }

  return SettingsPage::eventFilter(object, event);
}

void DeezerSettingsPage::LogoutClicked() {
  service_->Logout();
  ui_->button_login->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedOut);
}

void DeezerSettingsPage::LoginSuccess() {
  if (!this->isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedIn);
  ui_->button_login->setEnabled(false);
}

void DeezerSettingsPage::LoginFailure(QString failure_reason) {
  if (!this->isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
}
