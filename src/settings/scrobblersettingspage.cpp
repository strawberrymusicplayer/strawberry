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

#include "scrobblersettingspage.h"
#include "ui_scrobblersettingspage.h"

#include <QObject>
#include <QMessageBox>
#include <QSettings>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include "settingsdialog.h"
#include "settingspage.h"
#include "core/iconloader.h"
#include "core/song.h"
#include "core/settings.h"
#include "widgets/loginstatewidget.h"

#include "scrobbler/audioscrobbler.h"
#include "scrobbler/lastfmscrobbler.h"
#include "scrobbler/librefmscrobbler.h"
#include "scrobbler/listenbrainzscrobbler.h"
#include "constants/scrobblersettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace ScrobblerSettings;

ScrobblerSettingsPage::ScrobblerSettingsPage(SettingsDialog *dialog, const SharedPtr<AudioScrobbler> scrobbler, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_ScrobblerSettingsPage),
      scrobbler_(scrobbler),
      lastfmscrobbler_(scrobbler_->Service<LastFMScrobbler>()),
      librefmscrobbler_(scrobbler_->Service<LibreFMScrobbler>()),
      listenbrainzscrobbler_(scrobbler_->Service<ListenBrainzScrobbler>()),
      lastfm_waiting_for_auth_(false),
      librefm_waiting_for_auth_(false),
      listenbrainz_waiting_for_auth_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"scrobble"_s, true, 0, 32));

  // Last.fm
  QObject::connect(&*lastfmscrobbler_, &LastFMScrobbler::AuthenticationComplete, this, &ScrobblerSettingsPage::LastFM_AuthenticationComplete);
  QObject::connect(ui_->button_lastfm_login, &QPushButton::clicked, this, &ScrobblerSettingsPage::LastFM_Login);
  QObject::connect(ui_->widget_lastfm_login_state, &LoginStateWidget::LoginClicked, this, &ScrobblerSettingsPage::LastFM_Login);
  QObject::connect(ui_->widget_lastfm_login_state, &LoginStateWidget::LogoutClicked, this, &ScrobblerSettingsPage::LastFM_Logout);
  ui_->widget_lastfm_login_state->AddCredentialGroup(ui_->widget_lastfm_login);

  // Libre.fm
  QObject::connect(&*librefmscrobbler_, &LibreFMScrobbler::AuthenticationComplete, this, &ScrobblerSettingsPage::LibreFM_AuthenticationComplete);
  QObject::connect(ui_->button_librefm_login, &QPushButton::clicked, this, &ScrobblerSettingsPage::LibreFM_Login);
  QObject::connect(ui_->widget_librefm_login_state, &LoginStateWidget::LoginClicked, this, &ScrobblerSettingsPage::LibreFM_Login);
  QObject::connect(ui_->widget_librefm_login_state, &LoginStateWidget::LogoutClicked, this, &ScrobblerSettingsPage::LibreFM_Logout);
  ui_->widget_librefm_login_state->AddCredentialGroup(ui_->widget_librefm_login);

  // ListenBrainz
  QObject::connect(&*listenbrainzscrobbler_, &ListenBrainzScrobbler::AuthenticationComplete, this, &ScrobblerSettingsPage::ListenBrainz_AuthenticationComplete);
  QObject::connect(ui_->button_listenbrainz_login, &QPushButton::clicked, this, &ScrobblerSettingsPage::ListenBrainz_Login);
  QObject::connect(ui_->widget_listenbrainz_login_state, &LoginStateWidget::LoginClicked, this, &ScrobblerSettingsPage::ListenBrainz_Login);
  QObject::connect(ui_->widget_listenbrainz_login_state, &LoginStateWidget::LogoutClicked, this, &ScrobblerSettingsPage::ListenBrainz_Logout);
  ui_->widget_listenbrainz_login_state->AddCredentialGroup(ui_->widget_listenbrainz_login);

  ui_->label_listenbrainz_token->setText(u"<html><head/><body><p>"_s + tr("Enter your user token from") + QLatin1Char(' ') + u"<a href=\"https://listenbrainz.org/profile/\"><span style=\"text-decoration: underline; color:#0000ff;\">https://listenbrainz.org/profile/</span></a></p></body></html>"_s);

  resize(sizeHint());

}

ScrobblerSettingsPage::~ScrobblerSettingsPage() { delete ui_; }

void ScrobblerSettingsPage::Load() {

  Settings s;
  if (!s.contains(kSettingsGroup)) set_changed();

  ui_->checkbox_enable->setChecked(scrobbler_->enabled());
  ui_->checkbox_scrobble_button->setChecked(scrobbler_->scrobble_button());
  ui_->checkbox_love_button->setChecked(scrobbler_->love_button());
  ui_->checkbox_offline->setChecked(scrobbler_->offline());
  ui_->spinbox_submit->setValue(scrobbler_->submit_delay());
  ui_->checkbox_albumartist->setChecked(scrobbler_->prefer_albumartist());
  ui_->checkbox_show_error_dialog->setChecked(scrobbler_->ShowErrorDialog());
  ui_->checkbox_strip_remastered->setChecked(scrobbler_->strip_remastered());

  ui_->checkbox_source_collection->setChecked(scrobbler_->sources().contains(Song::Source::Collection));
  ui_->checkbox_source_local->setChecked(scrobbler_->sources().contains(Song::Source::LocalFile));
  ui_->checkbox_source_cdda->setChecked(scrobbler_->sources().contains(Song::Source::CDDA));
  ui_->checkbox_source_device->setChecked(scrobbler_->sources().contains(Song::Source::Device));
  ui_->checkbox_source_subsonic->setChecked(scrobbler_->sources().contains(Song::Source::Subsonic));
  ui_->checkbox_source_tidal->setChecked(scrobbler_->sources().contains(Song::Source::Tidal));
  ui_->checkbox_source_qobuz->setChecked(scrobbler_->sources().contains(Song::Source::Qobuz));
  ui_->checkbox_source_spotify->setChecked(scrobbler_->sources().contains(Song::Source::Spotify));
  ui_->checkbox_source_stream->setChecked(scrobbler_->sources().contains(Song::Source::Stream));
  ui_->checkbox_source_somafm->setChecked(scrobbler_->sources().contains(Song::Source::SomaFM));
  ui_->checkbox_source_radioparadise->setChecked(scrobbler_->sources().contains(Song::Source::RadioParadise));
  ui_->checkbox_source_unknown->setChecked(scrobbler_->sources().contains(Song::Source::Unknown));

  ui_->checkbox_lastfm_enable->setChecked(lastfmscrobbler_->enabled());
  LastFM_RefreshControls(lastfmscrobbler_->authenticated());

  ui_->checkbox_librefm_enable->setChecked(librefmscrobbler_->enabled());
  LibreFM_RefreshControls(librefmscrobbler_->authenticated());

  ui_->checkbox_listenbrainz_enable->setChecked(listenbrainzscrobbler_->enabled());
  ui_->lineedit_listenbrainz_user_token->setText(listenbrainzscrobbler_->user_token());
  ListenBrainz_RefreshControls(listenbrainzscrobbler_->authenticated());

  Init(ui_->layout_scrobblersettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void ScrobblerSettingsPage::Save() {

  Settings s;

  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->checkbox_enable->isChecked());
  s.setValue(kScrobbleButton, ui_->checkbox_scrobble_button->isChecked());
  s.setValue(kLoveButton, ui_->checkbox_love_button->isChecked());
  s.setValue(kOffline, ui_->checkbox_offline->isChecked());
  s.setValue(kSubmit, ui_->spinbox_submit->value());
  s.setValue(kAlbumArtist, ui_->checkbox_albumartist->isChecked());
  s.setValue(kShowErrorDialog, ui_->checkbox_show_error_dialog->isChecked());
  s.setValue(kStripRemastered, ui_->checkbox_strip_remastered->isChecked());

  QStringList sources;
  if (ui_->checkbox_source_collection->isChecked()) sources << Song::TextForSource(Song::Source::Collection);
  if (ui_->checkbox_source_local->isChecked()) sources << Song::TextForSource(Song::Source::LocalFile);
  if (ui_->checkbox_source_cdda->isChecked()) sources << Song::TextForSource(Song::Source::CDDA);
  if (ui_->checkbox_source_device->isChecked()) sources << Song::TextForSource(Song::Source::Device);
  if (ui_->checkbox_source_subsonic->isChecked()) sources << Song::TextForSource(Song::Source::Subsonic);
  if (ui_->checkbox_source_tidal->isChecked()) sources << Song::TextForSource(Song::Source::Tidal);
  if (ui_->checkbox_source_qobuz->isChecked()) sources << Song::TextForSource(Song::Source::Qobuz);
  if (ui_->checkbox_source_spotify->isChecked()) sources << Song::TextForSource(Song::Source::Spotify);
  if (ui_->checkbox_source_stream->isChecked()) sources << Song::TextForSource(Song::Source::Stream);
  if (ui_->checkbox_source_somafm->isChecked()) sources << Song::TextForSource(Song::Source::SomaFM);
  if (ui_->checkbox_source_radioparadise->isChecked()) sources << Song::TextForSource(Song::Source::RadioParadise);
  if (ui_->checkbox_source_unknown->isChecked()) sources << Song::TextForSource(Song::Source::Unknown);

  s.setValue(kSources, sources);

  s.endGroup();

  s.beginGroup(LastFMScrobbler::kSettingsGroup);
  s.setValue(kEnabled, ui_->checkbox_lastfm_enable->isChecked());
  s.endGroup();

  s.beginGroup(LibreFMScrobbler::kSettingsGroup);
  s.setValue(kEnabled, ui_->checkbox_librefm_enable->isChecked());
  s.endGroup();

  s.beginGroup(ListenBrainzScrobbler::kSettingsGroup);
  s.setValue(kEnabled, ui_->checkbox_listenbrainz_enable->isChecked());
  s.setValue(kUserToken, ui_->lineedit_listenbrainz_user_token->text());
  s.endGroup();

  scrobbler_->ReloadSettings();

}

void ScrobblerSettingsPage::LastFM_Login() {

  lastfm_waiting_for_auth_ = true;
  ui_->widget_lastfm_login_state->SetLoggedIn(LoginStateWidget::State::LoginInProgress);
  lastfmscrobbler_->Authenticate();

}

void ScrobblerSettingsPage::LastFM_Logout() {

  lastfmscrobbler_->ClearSession();
  LastFM_RefreshControls(false);

}

void ScrobblerSettingsPage::LastFM_AuthenticationComplete(const bool success, const QString &error) {

  if (!lastfm_waiting_for_auth_) return;
  lastfm_waiting_for_auth_ = false;

  if (success) {
    Save();
  }
  else {
    if (!error.isEmpty()) QMessageBox::warning(this, u"Authentication failed"_s, error);
  }

  LastFM_RefreshControls(success);

}

void ScrobblerSettingsPage::LastFM_RefreshControls(const bool authenticated) {
  ui_->widget_lastfm_login_state->SetLoggedIn(authenticated ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut, lastfmscrobbler_->username());
}

void ScrobblerSettingsPage::LibreFM_Login() {

  librefm_waiting_for_auth_ = true;
  ui_->widget_librefm_login_state->SetLoggedIn(LoginStateWidget::State::LoginInProgress);
  librefmscrobbler_->Authenticate();

}

void ScrobblerSettingsPage::LibreFM_Logout() {

  librefmscrobbler_->ClearSession();
  LibreFM_RefreshControls(false);

}

void ScrobblerSettingsPage::LibreFM_AuthenticationComplete(const bool success, const QString &error) {

  if (!librefm_waiting_for_auth_) return;
  librefm_waiting_for_auth_ = false;

  if (success) {
    Save();
  }
  else {
    QMessageBox::warning(this, u"Authentication failed"_s, error);
  }

  LibreFM_RefreshControls(success);

}

void ScrobblerSettingsPage::LibreFM_RefreshControls(const bool authenticated) {
  ui_->widget_librefm_login_state->SetLoggedIn(authenticated ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut, librefmscrobbler_->username());
}

void ScrobblerSettingsPage::ListenBrainz_Login() {

  listenbrainz_waiting_for_auth_ = true;
  ui_->widget_listenbrainz_login_state->SetLoggedIn(LoginStateWidget::State::LoginInProgress);
  listenbrainzscrobbler_->Authenticate();

}

void ScrobblerSettingsPage::ListenBrainz_Logout() {

  listenbrainzscrobbler_->Logout();
  ListenBrainz_RefreshControls(false);

}

void ScrobblerSettingsPage::ListenBrainz_AuthenticationComplete(const bool success, const QString &error) {

  if (!listenbrainz_waiting_for_auth_) return;
  listenbrainz_waiting_for_auth_ = false;

  if (success) {
    Save();
  }
  else {
    QMessageBox::warning(this, u"Authentication failed"_s, error);
  }

  ListenBrainz_RefreshControls(success);

}

void ScrobblerSettingsPage::ListenBrainz_RefreshControls(const bool authenticated) {
  ui_->widget_listenbrainz_login_state->SetLoggedIn(authenticated ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
}
