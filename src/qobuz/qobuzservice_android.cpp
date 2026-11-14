// SPDX-License-Identifier: GPL-3.0-or-later

#include "qobuzservice.h"

#include <QNetworkReply>

#include "core/browserauthentication_android.h"
#include "core/localredirectserver.h"

bool QobuzService::StartAndroidAuthentication(QString &error) {

  if (!android_browser_authentication_) {
    android_browser_authentication_ = new AndroidBrowserAuthentication(this);
    QObject::connect(android_browser_authentication_, &AndroidBrowserAuthentication::Failed, this, [this](const QString &failure) {
      LoginError(failure);
    });
  }
  return android_browser_authentication_->Start(QStringLiteral("Qobuz"), error);

}

bool QobuzService::AndroidAuthenticationInProgress() const {
  return android_browser_authentication_ && android_browser_authentication_->active();
}

bool QobuzService::CheckAndroidAuthentication() {
  if (AndroidAuthenticationInProgress() && !android_browser_authentication_->timed_out()) return true;
  LoginError(tr("Browser authentication timed out. Please try again."));
  return false;
}

void QobuzService::CancelAndroidAuthentication() {
  LoginError(tr("Authentication cancelled."));
}

void QobuzService::StopAndroidAuthentication() {

  if (local_redirect_server_) {
    QObject::disconnect(local_redirect_server_, nullptr, this, nullptr);
    local_redirect_server_->close();
    local_redirect_server_->deleteLater();
    local_redirect_server_ = nullptr;
  }
  if (android_login_reply_) {
    QNetworkReply *reply = android_login_reply_;
    android_login_reply_.clear();
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }
  if (android_browser_authentication_) android_browser_authentication_->Stop();

}
