/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef LISTENBRAINZSCROBBLER_H
#define LISTENBRAINZSCROBBLER_H

#include "config.h"

#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QQueue>
#include <QUrl>
#include <QJsonDocument>
#include <QElapsedTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"
#include "scrobblemetadata.h"

class QTimer;
class QNetworkReply;

class ScrobblerSettingsService;
class NetworkAccessManager;
class OAuthenticator;

class ListenBrainzScrobbler : public ScrobblerService {
  Q_OBJECT

 public:
  explicit ListenBrainzScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  static const char *kName;
  static const char *kSettingsGroup;

  static bool HasCompiledCredentials();

  void ReloadSettings() override;

  bool enabled() const override { return enabled_; }
  bool authentication_required() const override { return true; }
  bool authenticated() const override;
  bool use_authorization_header() const override { return true; }
  QByteArray authorization_header() const override { return "Token " + user_token_.toUtf8(); }
  bool submitted() const override { return submitted_; }
  QString user_token() const { return user_token_; }

  void Authenticate();
  void Deauthenticate();
  void Logout();
  void UpdateNowPlaying(const Song &song) override;
  void ClearPlaying() override;
  void Scrobble(const Song &song) override;
  void Love() override;
  void Start(const bool initial = false) override;
  void Stop() override;

 Q_SIGNALS:
  void AuthenticationComplete(const bool success, const QString &error = QString());

 public Q_SLOTS:
  void WriteCache() override { cache_->WriteCache(); }

 private:
  // A love waiting to be sent, along with the number of times sending it has already failed.
  struct LoveRequest {
    explicit LoveRequest(const QString &_recording_mbid) : recording_mbid(_recording_mbid), attempts(0) {}
    QString recording_mbid;
    int attempts;
  };

 private Q_SLOTS:
  void OAuthFinished(const bool success, const QString &error = QString(), const bool invalid_grant = false);
  void UpdateNowPlayingRequestFinished(QNetworkReply *reply);
  void ScrobbleRequestFinished(QNetworkReply *reply, ScrobblerCacheItemPtrList cache_items);
  void LoveRequestFinished(QNetworkReply *reply, LoveRequest love_request);

 private:
  QNetworkReply *CreateRequest(const QUrl &url, const QJsonDocument &json_document);
  QJsonObject JsonTrackMetadata(const ScrobbleMetadata &metadata) const;
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;
  void CheckScrobblePrevSong();
  void FlushRequests();
  void ScheduleFlushRequests();
  qint64 NextFlushRequestsDelay() const;
  void SubmitListens();
  void SendNowPlaying(const Song &song);
  void ReserveRequestSlot();
  void SendLove(const LoveRequest &love_request);
  void UpdateRateLimit(QNetworkReply *reply);
  qint64 RateLimitDelay() const;

  const SharedPtr<NetworkAccessManager> network_;
  OAuthenticator *oauth_;
  ScrobblerCache *cache_;
  QTimer *timer_flush_requests_;
  QElapsedTimer rate_limit_timer_;
  bool enabled_;
  QString client_id_;
  QString client_secret_;
  bool api_credentials_initialized_;
  QString user_token_;
  bool submitted_;
  Song song_playing_;
  bool scrobbled_;
  quint64 timestamp_;
  bool submit_error_;
  qint64 next_request_time_;
  QQueue<LoveRequest> queue_love_;
  bool now_playing_pending_;
  qint64 scrobbles_due_time_;

  bool prefer_albumartist_;
};

#endif  // LISTENBRAINZSCROBBLER_H
