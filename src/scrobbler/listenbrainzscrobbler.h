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

#ifndef LISTENBRAINZSCROBBLER_H
#define LISTENBRAINZSCROBBLER_H

#include "config.h"

#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QJsonDocument>

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
  void Submit() override;
  void UpdateNowPlaying(const Song &song) override;
  void ClearPlaying() override;
  void Scrobble(const Song &song) override;
  void Love() override;

 Q_SIGNALS:
  void AuthenticationComplete(const bool success, const QString &error = QString());

 public Q_SLOTS:
  void WriteCache() override { cache_->WriteCache(); }

 private Q_SLOTS:
  void OAuthFinished(const bool success, const QString &error);
  void UpdateNowPlayingRequestFinished(QNetworkReply *reply);
  void ScrobbleRequestFinished(QNetworkReply *reply, ScrobblerCacheItemPtrList cache_items);
  void LoveRequestFinished(QNetworkReply *reply);

 private:
  QNetworkReply *CreateRequest(const QUrl &url, const QJsonDocument &json_document);
  QJsonObject JsonTrackMetadata(const ScrobbleMetadata &metadata) const;
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;
  void StartSubmit(const bool initial = false) override;
  void CheckScrobblePrevSong();

  const SharedPtr<NetworkAccessManager> network_;
  OAuthenticator *oauth_;
  ScrobblerCache *cache_;
  QTimer *timer_submit_;
  bool enabled_;
  QString user_token_;
  bool submitted_;
  Song song_playing_;
  bool scrobbled_;
  quint64 timestamp_;
  bool submit_error_;

  bool prefer_albumartist_;
};

#endif  // LISTENBRAINZSCROBBLER_H
