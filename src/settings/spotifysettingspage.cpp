/*
 * Strawberry Music Player
 * Copyright 2022-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

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

#include "settingsdialog.h"
#include "spotifysettingspage.h"
#include "ui_spotifysettingspage.h"
#include "core/application.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "streaming/streamingservices.h"
#include "spotify/spotifyservice.h"
#include "widgets/loginstatewidget.h"

const char *SpotifySettingsPage::kSettingsGroup = "Spotify";

SpotifySettingsPage::SpotifySettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::SpotifySettingsPage),
      service_(dialog->app()->streaming_services()->Service<SpotifyService>()) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(QStringLiteral("spotify")));

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &SpotifySettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &SpotifySettingsPage::LogoutClicked);

  QObject::connect(this, &SpotifySettingsPage::Authorize, &*service_, &SpotifyService::Authenticate);

  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &SpotifySettingsPage::LoginFailure);
  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &SpotifySettingsPage::LoginSuccess);

  dialog->installEventFilter(this);

  GstRegistry *reg = gst_registry_get();
  if (reg) {
    GstPluginFeature *spotifyaudiosrc = gst_registry_lookup_feature(reg, "spotifyaudiosrc");
    if (spotifyaudiosrc) {
      gst_object_unref(spotifyaudiosrc);
      ui_->widget_warning->hide();
    }
    else {
      ui_->widget_warning->show();
    }
  }

}

SpotifySettingsPage::~SpotifySettingsPage() { delete ui_; }

void SpotifySettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value("enabled", false).toBool());

  ui_->username->setText(s.value("username").toString());
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));

  ui_->searchdelay->setValue(s.value("searchdelay", 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value("artistssearchlimit", 4).toInt());
  ui_->albumssearchlimit->setValue(s.value("albumssearchlimit", 10).toInt());
  ui_->songssearchlimit->setValue(s.value("songssearchlimit", 10).toInt());
  ui_->checkbox_fetchalbums->setChecked(s.value("fetchalbums", false).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value("downloadalbumcovers", true).toBool());

  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_spotifysettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void SpotifySettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->enable->isChecked());

  s.setValue("username", ui_->username->text());
  s.setValue("password", QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));

  s.setValue("searchdelay", ui_->searchdelay->value());
  s.setValue("artistssearchlimit", ui_->artistssearchlimit->value());
  s.setValue("albumssearchlimit", ui_->albumssearchlimit->value());
  s.setValue("songssearchlimit", ui_->songssearchlimit->value());
  s.setValue("fetchalbums", ui_->checkbox_fetchalbums->isChecked());
  s.setValue("downloadalbumcovers", ui_->checkbox_download_album_covers->isChecked());
  s.endGroup();

}

void SpotifySettingsPage::LoginClicked() {

  Q_EMIT Authorize();

  ui_->button_login->setEnabled(false);

}

bool SpotifySettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void SpotifySettingsPage::LogoutClicked() {

  service_->Deauthenticate();
  ui_->button_login->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);

}

void SpotifySettingsPage::LoginSuccess() {

  if (!isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_login->setEnabled(true);

}

void SpotifySettingsPage::LoginFailure(const QString &failure_reason) {

  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
  ui_->button_login->setEnabled(true);

}
