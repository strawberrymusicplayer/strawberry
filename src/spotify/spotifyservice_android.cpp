// SPDX-License-Identifier: GPL-3.0-or-later

#include "spotifyservice.h"

#include "core/browserauthentication_android.h"
#include "core/oauthenticator.h"

bool SpotifyService::StartAndroidAuthentication(QString &error) {

  if (!android_browser_authentication_) {
    android_browser_authentication_ = new AndroidBrowserAuthentication(this);
    QObject::connect(android_browser_authentication_, &AndroidBrowserAuthentication::Failed, this, [this](const QString &failure) {
      oauth_->CancelAuthentication();
      OAuthFinished(false, failure, false);
    });
  }
  return android_browser_authentication_->Start(QStringLiteral("Spotify"), error);

}

bool SpotifyService::AndroidAuthenticationInProgress() const {
  return android_browser_authentication_ && android_browser_authentication_->active();
}

void SpotifyService::CancelAndroidAuthentication() {
  if (!AndroidAuthenticationInProgress()) return;
  oauth_->CancelAuthentication();
  StopAndroidAuthentication();
  Q_EMIT LoginFinished(false);
}

void SpotifyService::StopAndroidAuthentication() {
  if (android_browser_authentication_) android_browser_authentication_->Stop();
}
