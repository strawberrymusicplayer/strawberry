// SPDX-License-Identifier: GPL-3.0-or-later

#include "tidalservice.h"

#include "core/browserauthentication_android.h"
#include "core/oauthenticator.h"

bool TidalService::StartAndroidAuthentication(QString &error) {

  if (!android_browser_authentication_) {
    android_browser_authentication_ = new AndroidBrowserAuthentication(this);
    QObject::connect(android_browser_authentication_, &AndroidBrowserAuthentication::Failed, this, [this](const QString &failure) {
      oauth_->CancelAuthentication();
      Q_EMIT LoginFailure(failure);
      Q_EMIT LoginFinished(false);
    });
  }
  return android_browser_authentication_->Start(QStringLiteral("Tidal"), error);

}

bool TidalService::AndroidAuthenticationInProgress() const {

  return android_browser_authentication_ && android_browser_authentication_->active();

}

void TidalService::StopAndroidAuthentication() {

  if (android_browser_authentication_) android_browser_authentication_->Stop();

}
