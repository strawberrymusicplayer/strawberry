/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include "core/iconloader.h"
#include "core/settings.h"
#include "spotify/spotifyservice.h"
#include "widgets/loginstatewidget.h"
#include "constants/spotifysettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace SpotifySettings;

SpotifySettingsPage::SpotifySettingsPage(SettingsDialog *dialog, const SharedPtr<SpotifyService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::SpotifySettingsPage),
      service_(service) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"spotify"_s));

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

void SpotifySettingsPage::showEvent(QShowEvent *e) {

  ui_->login_state->SetLoggedIn(service_->authenticated() ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
  SettingsPage::showEvent(e);

}

void SpotifySettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value(kEnabled, false).toBool());

  ui_->searchdelay->setValue(s.value(kSearchDelay, 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value(kArtistsSearchLimit, 4).toInt());
  ui_->albumssearchlimit->setValue(s.value(kAlbumsSearchLimit, 10).toInt());
  ui_->songssearchlimit->setValue(s.value(kSongsSearchLimit, 10).toInt());
  ui_->checkbox_fetchalbums->setChecked(s.value(kFetchAlbums, false).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, true).toBool());

  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_spotifysettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void SpotifySettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->enable->isChecked());
  s.setValue(kSearchDelay, ui_->searchdelay->value());
  s.setValue(kArtistsSearchLimit, ui_->artistssearchlimit->value());
  s.setValue(kAlbumsSearchLimit, ui_->albumssearchlimit->value());
  s.setValue(kSongsSearchLimit, ui_->songssearchlimit->value());
  s.setValue(kFetchAlbums, ui_->checkbox_fetchalbums->isChecked());
  s.setValue(kDownloadAlbumCovers, ui_->checkbox_download_album_covers->isChecked());
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

  service_->ClearSession();
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
