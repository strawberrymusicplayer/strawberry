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

#ifndef LISTENBRAINZSCROBBLER_H
#define LISTENBRAINZSCROBBLER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QJsonDocument>

#include "core/song.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"

class QNetworkReply;

class Application;
class NetworkAccessManager;
class LocalRedirectServer;

class ListenBrainzScrobbler : public ScrobblerService {
  Q_OBJECT

 public:
  explicit ListenBrainzScrobbler(Application *app, QObject *parent = nullptr);
  ~ListenBrainzScrobbler();

  static const char *kName;
  static const char *kSettingsGroup;

  void ReloadSettings();
  void LoadSession();

  bool IsEnabled() const { return enabled_; }
  bool IsAuthenticated() const { return !access_token_.isEmpty() && !user_token_.isEmpty(); }
  bool IsSubmitted() const { return submitted_; }
  void Submitted() { submitted_ = true; }
  QString user_token() const { return user_token_; }

  void Authenticate(const bool https = false);
  void Logout();
  void ShowConfig();
  void Submit();
  void UpdateNowPlaying(const Song &song);
  void ClearPlaying();
  void Scrobble(const Song &song);

 signals:
  void AuthenticationComplete(bool success, QString error = QString());

 public slots:
  void WriteCache() { cache_->WriteCache(); }

 private slots:
  void RedirectArrived();
  void AuthenticateReplyFinished(QNetworkReply *reply);
  void UpdateNowPlayingRequestFinished(QNetworkReply *reply);
  void ScrobbleRequestFinished(QNetworkReply *reply, QList<quint64>);

 private:
  QNetworkReply *CreateRequest(const QUrl &url, const QJsonDocument &json_doc);
  QByteArray GetReplyData(QNetworkReply *reply);

  void RequestSession(const QUrl &url, const QString &token);
  void AuthError(const QString &error);
  void Error(const QString &error, const QVariant &debug = QVariant());
  void DoSubmit();

  static const char *kAuthUrl;
  static const char *kAuthTokenUrl;
  static const char *kApiUrl;
  static const char *kClientID;
  static const char *kClientSecret;
  static const char *kCacheFile;
  static const char *kRedirectUrl;
  static const int kScrobblesPerRequest;

  Application *app_;
  NetworkAccessManager *network_;
  ScrobblerCache *cache_;
  LocalRedirectServer *server_;
  bool enabled_;
  QString user_token_;
  QString access_token_;
  qint64 expires_in_;
  QString token_type_;
  QString refresh_token_;
  bool submitted_;
  Song song_playing_;
  quint64 timestamp_;

};

#endif  // LISTENBRAINZSCROBBLER_H
