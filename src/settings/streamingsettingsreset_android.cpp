// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include <QObject>

#ifdef HAVE_PLEX
#include "plexsettingspage.h"
#include "ui_plexsettingspage.h"

void PlexSettingsPage::ResetAndroidSettings() {

  servers_.clear();
  ui_->server_url->clear();
  login_pending_ = false;
  test_pending_ = false;
  ui_->button_test->setEnabled(true);
  ui_->button_refresh_servers->setEnabled(true);
  Load();

}
#endif

#ifdef HAVE_QOBUZ
#include "qobuzsettingspage.h"
#include "qobuz/qobuzapicredentialfetcher.h"
#include "ui_qobuzsettingspage.h"

void QobuzSettingsPage::ResetAndroidSettings() {

  delete api_credential_fetcher_;
  api_credential_fetcher_ = nullptr;
  ui_->button_fetch_api_credentials->setEnabled(true);
  ui_->button_fetch_api_credentials->setText(tr("Fetch API Credentials"));
  Load();

}
#endif

#ifdef HAVE_JELLYFIN
#include "jellyfinsettingspage.h"

void JellyfinSettingsPage::ResetAndroidSettings() {

  if (save_password_reply_) {
    QObject::disconnect(&*save_password_reply_, nullptr, this, nullptr);
    save_password_reply_.reset();
  }
  password_save_failed_ = false;
  unsaved_password_.clear();
  saving_password_.clear();
  Load();

}
#endif
