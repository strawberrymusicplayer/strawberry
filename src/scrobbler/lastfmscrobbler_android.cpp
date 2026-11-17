// SPDX-License-Identifier: GPL-3.0-or-later

#include "lastfmscrobbler.h"

#include "core/browserauthentication_android.h"

bool LastFMScrobbler::StartAndroidAuthentication(QString &error) {

  if (!android_browser_authentication_) {
    android_browser_authentication_ = new AndroidBrowserAuthentication(this);
    QObject::connect(android_browser_authentication_, &AndroidBrowserAuthentication::Failed, this, [this](const QString &failure) {
      AuthError(failure);
    });
  }
  return android_browser_authentication_->Start(QStringLiteral("Last.fm"), error);

}

bool LastFMScrobbler::AndroidAuthenticationInProgress() const {
  return android_browser_authentication_ && android_browser_authentication_->active();
}

bool LastFMScrobbler::CheckAndroidAuthentication() {
  if (AndroidAuthenticationInProgress() && !android_browser_authentication_->timed_out()) return true;
  AuthError(tr("Browser authentication timed out. Please try again."));
  return false;
}

void LastFMScrobbler::CancelAndroidAuthentication() {
  AuthError(tr("Authentication cancelled."));
}

void LastFMScrobbler::StopAndroidAuthentication() {
  if (android_browser_authentication_) android_browser_authentication_->Stop();
}
