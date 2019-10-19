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

#include "scrobblersettingspage.h"
#include "ui_scrobblersettingspage.h"

#include <QObject>
#include <QMessageBox>
#include <QSettings>

#include "core/application.h"
#include "core/iconloader.h"

#include "scrobbler/audioscrobbler.h"
#include "scrobbler/scrobblerservice.h"
#include "scrobbler/lastfmscrobbler.h"
#include "scrobbler/librefmscrobbler.h"
#include "scrobbler/listenbrainzscrobbler.h"

const char *ScrobblerSettingsPage::kSettingsGroup = "Scrobbler";

ScrobblerSettingsPage::ScrobblerSettingsPage(SettingsDialog *parent)
    : SettingsPage(parent),
      scrobbler_(dialog()->app()->scrobbler()),
      lastfmscrobbler_(dialog()->app()->scrobbler()->Service<LastFMScrobbler>()),
      librefmscrobbler_(dialog()->app()->scrobbler()->Service<LibreFMScrobbler>()),
      listenbrainzscrobbler_(dialog()->app()->scrobbler()->Service<ListenBrainzScrobbler>()),
      ui_(new Ui_ScrobblerSettingsPage),
      lastfm_waiting_for_auth_(false),
      librefm_waiting_for_auth_(false),
      listenbrainz_waiting_for_auth_(false)
      {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("scrobble"));

  // Last.fm
  connect(lastfmscrobbler_, SIGNAL(AuthenticationComplete(bool, QString)), SLOT(LastFM_AuthenticationComplete(bool, QString)));
  connect(ui_->button_lastfm_login, SIGNAL(clicked()), SLOT(LastFM_Login()));
  connect(ui_->widget_lastfm_login_state, SIGNAL(LoginClicked()), SLOT(LastFM_Login()));
  connect(ui_->widget_lastfm_login_state, SIGNAL(LogoutClicked()), SLOT(LastFM_Logout()));
  ui_->widget_lastfm_login_state->AddCredentialGroup(ui_->widget_lastfm_login);

  // Libre.fm
  connect(librefmscrobbler_, SIGNAL(AuthenticationComplete(bool, QString)), SLOT(LibreFM_AuthenticationComplete(bool, QString)));
  connect(ui_->button_librefm_login, SIGNAL(clicked()), SLOT(LibreFM_Login()));
  connect(ui_->widget_librefm_login_state, SIGNAL(LoginClicked()), SLOT(LibreFM_Login()));
  connect(ui_->widget_librefm_login_state, SIGNAL(LogoutClicked()), SLOT(LibreFM_Logout()));
  ui_->widget_librefm_login_state->AddCredentialGroup(ui_->widget_librefm_login);

  // ListenBrainz
  connect(listenbrainzscrobbler_, SIGNAL(AuthenticationComplete(bool, QString)), SLOT(ListenBrainz_AuthenticationComplete(bool, QString)));
  connect(ui_->button_listenbrainz_login, SIGNAL(clicked()), SLOT(ListenBrainz_Login()));
  connect(ui_->widget_listenbrainz_login_state, SIGNAL(LoginClicked()), SLOT(ListenBrainz_Login()));
  connect(ui_->widget_listenbrainz_login_state, SIGNAL(LogoutClicked()), SLOT(ListenBrainz_Logout()));
  ui_->widget_listenbrainz_login_state->AddCredentialGroup(ui_->widget_listenbrainz_login);

  resize(sizeHint());

}

ScrobblerSettingsPage::~ScrobblerSettingsPage() { delete ui_; }

void ScrobblerSettingsPage::Load() {

  ui_->checkbox_enable->setChecked(scrobbler_->IsEnabled());
  ui_->checkbox_scrobble_button->setChecked(scrobbler_->ScrobbleButton());
  ui_->checkbox_love_button->setChecked(scrobbler_->LoveButton());
  ui_->checkbox_offline->setChecked(scrobbler_->IsOffline());
  ui_->spinbox_submit->setValue(scrobbler_->SubmitDelay());
  ui_->checkbox_albumartist->setChecked(scrobbler_->PreferAlbumArtist());

  ui_->checkbox_lastfm_enable->setChecked(lastfmscrobbler_->IsEnabled());
  ui_->checkbox_lastfm_https->setChecked(lastfmscrobbler_->IsUseHTTPS());
  LastFM_RefreshControls(lastfmscrobbler_->IsAuthenticated());

  ui_->checkbox_librefm_enable->setChecked(librefmscrobbler_->IsEnabled());
  LibreFM_RefreshControls(librefmscrobbler_->IsAuthenticated());

  ui_->checkbox_listenbrainz_enable->setChecked(listenbrainzscrobbler_->IsEnabled());
  ui_->lineedit_listenbrainz_user_token->setText(listenbrainzscrobbler_->user_token());
  ListenBrainz_RefreshControls(listenbrainzscrobbler_->IsAuthenticated());

}

void ScrobblerSettingsPage::Save() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->checkbox_enable->isChecked());
  s.setValue("scrobble_button", ui_->checkbox_scrobble_button->isChecked());
  s.setValue("love_button", ui_->checkbox_love_button->isChecked());
  s.setValue("offline", ui_->checkbox_offline->isChecked());
  s.setValue("submit", ui_->spinbox_submit->value());
  s.setValue("albumartist", ui_->checkbox_albumartist->isChecked());
  s.endGroup();

  s.beginGroup(LastFMScrobbler::kSettingsGroup);
  s.setValue("enabled", ui_->checkbox_lastfm_enable->isChecked());
  s.setValue("https", ui_->checkbox_lastfm_https->isChecked());
  s.endGroup();

  s.beginGroup(LibreFMScrobbler::kSettingsGroup);
  s.setValue("enabled", ui_->checkbox_librefm_enable->isChecked());
  s.endGroup();

  s.beginGroup(ListenBrainzScrobbler::kSettingsGroup);
  s.setValue("enabled", ui_->checkbox_listenbrainz_enable->isChecked());
  s.setValue("user_token", ui_->lineedit_listenbrainz_user_token->text());
  s.endGroup();

  scrobbler_->ReloadSettings();

}

void ScrobblerSettingsPage::LastFM_Login() {

  lastfm_waiting_for_auth_ = true;
  ui_->widget_lastfm_login_state->SetLoggedIn(LoginStateWidget::LoginInProgress);
  lastfmscrobbler_->Authenticate(ui_->checkbox_lastfm_https->isChecked());

}

void ScrobblerSettingsPage::LastFM_Logout() {

  lastfmscrobbler_->Logout();
  LastFM_RefreshControls(false);

}

void ScrobblerSettingsPage::LastFM_AuthenticationComplete(const bool success, QString error) {

  if (!lastfm_waiting_for_auth_) return;
  lastfm_waiting_for_auth_ = false;

  if (success) {
    Save();
  }
  else {
    if (!error.isEmpty()) QMessageBox::warning(this, "Authentication failed", error);
  }

  LastFM_RefreshControls(success);

}

void ScrobblerSettingsPage::LastFM_RefreshControls(bool authenticated) {
  ui_->widget_lastfm_login_state->SetLoggedIn(authenticated ? LoginStateWidget::LoggedIn : LoginStateWidget::LoggedOut, lastfmscrobbler_->username());
}

void ScrobblerSettingsPage::LibreFM_Login() {

  librefm_waiting_for_auth_ = true;
  ui_->widget_librefm_login_state->SetLoggedIn(LoginStateWidget::LoginInProgress);
  librefmscrobbler_->Authenticate();

}

void ScrobblerSettingsPage::LibreFM_Logout() {

  librefmscrobbler_->Logout();
  LibreFM_RefreshControls(false);

}

void ScrobblerSettingsPage::LibreFM_AuthenticationComplete(const bool success, QString error) {

  if (!librefm_waiting_for_auth_) return;
  librefm_waiting_for_auth_ = false;

  if (success) {
    Save();
  }
  else {
    QMessageBox::warning(this, "Authentication failed", error);
  }

  LibreFM_RefreshControls(success);

}

void ScrobblerSettingsPage::LibreFM_RefreshControls(bool authenticated) {
  ui_->widget_librefm_login_state->SetLoggedIn(authenticated ? LoginStateWidget::LoggedIn : LoginStateWidget::LoggedOut, librefmscrobbler_->username());
}

void ScrobblerSettingsPage::ListenBrainz_Login() {

  listenbrainz_waiting_for_auth_ = true;
  ui_->widget_listenbrainz_login_state->SetLoggedIn(LoginStateWidget::LoginInProgress);
  listenbrainzscrobbler_->Authenticate();

}

void ScrobblerSettingsPage::ListenBrainz_Logout() {

  listenbrainzscrobbler_->Logout();
  ListenBrainz_RefreshControls(false);

}

void ScrobblerSettingsPage::ListenBrainz_AuthenticationComplete(const bool success, QString error) {

  if (!listenbrainz_waiting_for_auth_) return;
  listenbrainz_waiting_for_auth_ = false;

  if (success) {
    Save();
  }
  else {
    QMessageBox::warning(this, "Authentication failed", error);
  }

  ListenBrainz_RefreshControls(success);

}

void ScrobblerSettingsPage::ListenBrainz_RefreshControls(bool authenticated) {
  ui_->widget_listenbrainz_login_state->SetLoggedIn(authenticated ? LoginStateWidget::LoggedIn : LoginStateWidget::LoggedOut);
}


