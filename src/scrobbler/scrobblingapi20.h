/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SCROBBLINGAPI20_H
#define SCROBBLINGAPI20_H

#include "config.h"

#include <QVariant>
#include <QByteArray>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"
#include "scrobblercacheitem.h"

class QTimer;
class QNetworkReply;

class ScrobblerSettingsService;
class NetworkAccessManager;
class LocalRedirectServer;

class ScrobblingAPI20 : public ScrobblerService {
  Q_OBJECT

 public:
  explicit ScrobblingAPI20(const QString &name, const QString &settings_group, const QString &auth_url, const QString &api_url, const bool batch, const QString &cache_file, const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~ScrobblingAPI20() override;

  static const char *kApiKey;

  void ReloadSettings() override;
  void LoadSession();
  void ClearSession();

  bool enabled() const override { return enabled_; }
  bool authentication_required() const override { return true; }
  bool authenticated() const override { return !username_.isEmpty() && !session_key_.isEmpty(); }
  bool use_authorization_header() const override { return false; }
  QByteArray authorization_header() const override { return QByteArray(); }

  bool subscriber() const { return subscriber_; }
  bool submitted() const override { return submitted_; }
  QString username() const { return username_; }

  void Authenticate();
  void UpdateNowPlaying(const Song &song) override;
  void ClearPlaying() override;
  void Scrobble(const Song &song) override;
  void Submit() override;
  void Love() override;

 Q_SIGNALS:
  void AuthenticationComplete(const bool success, const QString &error = QString());

 public Q_SLOTS:
  void WriteCache() override { cache_->WriteCache(); }

 private Q_SLOTS:
  void RedirectArrived();
  void AuthenticateReplyFinished(QNetworkReply *reply);
  void UpdateNowPlayingRequestFinished(QNetworkReply *reply);
  void ScrobbleRequestFinished(QNetworkReply *reply, ScrobblerCacheItemPtrList cache_items);
  void SingleScrobbleRequestFinished(QNetworkReply *reply, ScrobblerCacheItemPtr cache_item);
  void LoveRequestFinished(QNetworkReply *reply);

 private:
  enum class ScrobbleErrorCode {
    NoError = 1,
    InvalidService = 2,
    InvalidMethod = 3,
    AuthenticationFailed = 4,
    InvalidFormat = 5,
    InvalidParameters = 6,
    InvalidResourceSpecified = 7,
    OperationFailed = 8,
    InvalidSessionKey = 9,
    InvalidApiKey = 10,
    ServiceOffline = 11,
    SubscribersOnly = 12,
    InvalidMethodSignature = 13,
    UnauthorizedToken = 14,
    ItemUnavailable = 15,
    TemporarilyUnavailable = 16,
    LoginRequired = 17,
    TrialExpired = 18,
    ErrorDoesNotExist = 19,
    NotEnoughContent = 20,
    NotEnoughMembers = 21,
    NotEnoughFans = 22,
    NotEnoughNeighbours = 23,
    NoPeakRadio = 24,
    RadioNotFound = 25,
    APIKeySuspended = 26,
    Deprecated = 27,
    RateLimitExceeded = 29,
  };

  QNetworkReply *CreateRequest(const ParamList &request_params);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void RequestSession(const QString &token);
  void AuthError(const QString &error);
  void SendSingleScrobble(ScrobblerCacheItemPtr item);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
  static QString ErrorString(const ScrobbleErrorCode error);
  void StartSubmit(const bool initial = false) override;
  void CheckScrobblePrevSong();

 protected:
  QString name_;
  QString settings_group_;
  QString auth_url_;
  QString api_url_;
  bool batch_;

  const SharedPtr<NetworkAccessManager> network_;
  ScrobblerCache *cache_;
  LocalRedirectServer *local_redirect_server_;

  bool enabled_;
  bool prefer_albumartist_;

  bool subscriber_;
  QString username_;
  QString session_key_;

  bool submitted_;
  Song song_playing_;
  bool scrobbled_;
  quint64 timestamp_;
  bool submit_error_;

  QTimer *timer_submit_;
};

#endif  // SCROBBLINGAPI20_H
