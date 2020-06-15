/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>

#include "core/song.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"
#include "scrobblercacheitem.h"

class QNetworkReply;

class Application;
class NetworkAccessManager;
class LocalRedirectServer;

class ScrobblingAPI20 : public ScrobblerService {
  Q_OBJECT

 public:
  explicit ScrobblingAPI20(const QString &name, const QString &settings_group, const QString &auth_url, const QString &api_url, const bool batch, Application *app, QObject *parent = nullptr);
  ~ScrobblingAPI20() override;

  static const char *kRedirectUrl;

  void ReloadSettings() override;
  void LoadSession();
  
  virtual NetworkAccessManager *network() const = 0;
  virtual ScrobblerCache *cache() const = 0;

  bool IsEnabled() const override { return enabled_; }
  bool IsUseHTTPS() const { return https_; }
  bool IsAuthenticated() const override { return !username_.isEmpty() && !session_key_.isEmpty(); }
  bool IsSubscriber() const { return subscriber_; }
  bool IsSubmitted() const override { return submitted_; }
  void Submitted() override { submitted_ = true; }
  QString username() const { return username_; }

  void Authenticate(const bool https = false);
  void Logout();
  void UpdateNowPlaying(const Song &song) override;
  void ClearPlaying() override;
  void Scrobble(const Song &song) override;
  void Submit() override;
  void Love() override;

 signals:
  void AuthenticationComplete(bool success, QString error = QString());

 public slots:
  void WriteCache() override { cache()->WriteCache(); }

 private slots:
  void RedirectArrived();
  void AuthenticateReplyFinished(QNetworkReply *reply);
  void UpdateNowPlayingRequestFinished(QNetworkReply *reply);
  void ScrobbleRequestFinished(QNetworkReply *reply, QList<quint64>);
  void SingleScrobbleRequestFinished(QNetworkReply *reply, quint64 timestamp);
  void LoveRequestFinished(QNetworkReply *reply);

 private:

  enum ScrobbleErrorCode {
    Unknown = 0,
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
    Reserved12 = 12,
    InvalidMethodSignature = 13,
    Reserved14 = 14,
    Reserved15 = 15,
    TempError = 16,
    Reserved17 = 17,
    Reserved18 = 18,
    Reserved19 = 19,
    Reserved20 = 20,
    Reserved21 = 21,
    Reserved22 = 22,
    Reserved23 = 23,
    Reserved24 = 24,
    Reserved25 = 25,
    SuspendedAPIKey = 26,
    Reserved27 = 27,
    Reserved28 = 28,
    RateLimitExceeded = 29,
  };

  static const char *kApiKey;
  static const char *kSecret;
  static const int kScrobblesPerRequest;

  QNetworkReply *CreateRequest(const ParamList &request_params);
  QByteArray GetReplyData(QNetworkReply *reply);

  void RequestSession(const QString &token);
  void AuthError(const QString &error);
  void SendSingleScrobble(ScrobblerCacheItemPtr item);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
  QString ErrorString(const ScrobbleErrorCode error) const;
  void DoSubmit() override;
  void CheckScrobblePrevSong();

  QString name_;
  QString settings_group_;
  QString auth_url_;
  QString api_url_;
  bool batch_;

  Application *app_;
  LocalRedirectServer *server_;

  bool enabled_;
  bool https_;
  bool prefer_albumartist_;

  bool subscriber_;
  QString username_;
  QString session_key_;

  bool submitted_;
  Song song_playing_;
  bool scrobbled_;
  quint64 timestamp_;

  QList<QNetworkReply*> replies_;

};

#endif  // SCROBBLINGAPI20_H
