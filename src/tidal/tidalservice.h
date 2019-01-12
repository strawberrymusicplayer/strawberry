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

#ifndef TIDALSERVICE_H
#define TIDALSERVICE_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/song.h"
#include "internet/internetservices.h"
#include "internet/internetservice.h"
#include "internet/internetsearch.h"

class NetworkAccessManager;
class TidalUrlHandler;

class TidalService : public InternetService {
  Q_OBJECT

 public:
  TidalService(Application *app, QObject *parent);
  ~TidalService();

  static const Song::Source kSource;

  void ReloadSettings();

  void Logout();
  int Search(const QString &query, InternetSearch::SearchBy searchby);
  void CancelSearch();

  const bool login_sent() { return login_sent_; }
  const bool authenticated() { return (!session_id_.isEmpty() && !country_code_.isEmpty()); }

  void GetStreamURL(const QUrl &url);

 signals:
  void Login();
  void Login(const QString &username, const QString &password);
  void LoginSuccess();
  void LoginFailure(QString failure_reason);
  void SearchResults(int id, SongList songs);
  void SearchError(int id, QString message);
  void UpdateStatus(QString text);
  void ProgressSetMaximum(int max);
  void UpdateProgress(int max);
  void GetStreamURLFinished(QNetworkReply *reply, const QUrl url);
  void StreamURLFinished(const QUrl url, const Song::FileType, QString error = QString());

 public slots:
  void ShowConfig();
  void SendLogin(const QString &username, const QString &password);

 private slots:
  void SendLogin();
  void HandleAuthReply(QNetworkReply *reply);
  void ResetLoginAttempts();
  void StartSearch();
  void SearchFinished(QNetworkReply *reply, int search_id);
  void GetAlbumFinished(QNetworkReply *reply, int search_id, int album_id);
  void GetStreamURLFinished(QNetworkReply *reply, const int song_id, const QUrl original_url);

 private:
  void ClearSearch();
  void LoadSessionID();
  QNetworkReply *CreateRequest(const QString &ressource_name, const QList<QPair<QString, QString>> &params);
  QByteArray GetReplyData(QNetworkReply *reply, const bool sendlogin = false);
  QJsonObject ExtractJsonObj(QByteArray &data);
  QJsonValue ExtractItems(QByteArray &data);
  void SendSearch();
  void GetAlbum(const int album_id);
  Song ParseSong(const int album_id_requested, const QJsonValue &value);
  void CheckFinish();
  void Error(QString error, QVariant debug = QVariant());

  static const char *kApiUrl;
  static const char *kAuthUrl;
  static const char *kResourcesUrl;
  static const char *kApiTokenB64;
  static const int kLoginAttempts;
  static const int kTimeResetLoginAttempts;

  NetworkAccessManager *network_;
  TidalUrlHandler *url_handler_;
  QTimer *timer_searchdelay_;
  QTimer *timer_login_attempt_;

  QString username_;
  QString password_;
  QString quality_;
  int searchdelay_;
  int albumssearchlimit_;
  int songssearchlimit_;
  bool fetchalbums_;
  QString coversize_;
  QString streamurl_;
  QString session_id_;
  quint64 user_id_;
  QString country_code_;
  QString clientuniquekey_;

  int pending_search_id_;
  int next_pending_search_id_;
  QString pending_search_text_;
  InternetSearch::SearchBy pending_searchby_;

  int search_id_;
  QString search_text_;
  QHash<int, int> requests_album_;
  QHash<int, QUrl> requests_song_;
  int albums_requested_;
  int albums_received_;
  SongList songs_;
  QString search_error_;
  bool login_sent_;
  int login_attempts_;
  QUrl stream_request_url_;

};

#endif  // TIDALSERVICE_H
