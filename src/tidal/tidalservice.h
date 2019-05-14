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
#include <QList>
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

class Application;
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
  int Search(const QString &query, InternetSearch::SearchType type);
  void CancelSearch();

  const bool login_sent() { return login_sent_; }
  const bool authenticated() { return (!session_id_.isEmpty() && !country_code_.isEmpty()); }

  QString token() { return token_; }
  QString session_id() { return session_id_; }
  QString country_code() { return country_code_; }

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
  void StreamURLFinished(const QUrl original_url, const QUrl url, const Song::FileType, QString error = QString());

 public slots:
  void ShowConfig();
  void SendLogin(const QString &username, const QString &password, const QString &token);

 private slots:
  void SendLogin();
  void HandleAuthReply(QNetworkReply *reply);
  void ResetLoginAttempts();
  void StartSearch();
  void ArtistsReceived(QNetworkReply *reply, int search_id);
  void AlbumsReceived(QNetworkReply *reply, int search_id, int artist_id, int offset_requested = 0);
  void AlbumsFinished(const int artist_id, const int offset_requested, const int total_albums = 0, const int limit = 0, const int albums = 0);
  void SongsReceived(QNetworkReply *reply, int search_id, int album_id);
  void StreamURLReceived(QNetworkReply *reply, const int song_id, const QUrl original_url);

 private:
  typedef QPair<QString, QString> Param;

  void ClearSearch();
  void LoadSessionID();
  QNetworkReply *CreateRequest(const QString &ressource_name, const QList<QPair<QString, QString>> &params);
  QByteArray GetReplyData(QNetworkReply *reply, QString &error, const bool sendlogin = false);
  QJsonObject ExtractJsonObj(QByteArray &data, QString &error);
  QJsonValue ExtractItems(QByteArray &data, QString &error);
  QJsonValue ExtractItems(QJsonObject &json_obj, QString &error);
  void SendSearch();
  void SendArtistsSearch();
  void SendAlbumsSearch();
  void SendSongsSearch();
  void GetAlbums(const int artist_id, const int offset = 0);
  void GetSongs(const int album_id);
  Song ParseSong(const int album_id_requested, const QJsonValue &value, QString album_artist = QString());
  void CheckFinish();
  QString LoginError(QString error, QVariant debug = QVariant());
  QString Error(QString error, QVariant debug = QVariant());

  static const char *kApiUrl;
  static const char *kAuthUrl;
  static const char *kResourcesUrl;
  static const char *kApiTokenB64;
  static const int kLoginAttempts;
  static const int kTimeResetLoginAttempts;

  Application *app_;
  NetworkAccessManager *network_;
  TidalUrlHandler *url_handler_;
  QTimer *timer_search_delay_;
  QTimer *timer_login_attempt_;

  QString token_;
  QString username_;
  QString password_;
  QString quality_;
  int search_delay_;
  int artistssearchlimit_;
  int albumssearchlimit_;
  int songssearchlimit_;
  bool fetchalbums_;
  QString coversize_;
  QString session_id_;
  quint64 user_id_;
  QString country_code_;

  int pending_search_id_;
  int next_pending_search_id_;
  QString pending_search_text_;
  InternetSearch::SearchType pending_search_type_;

  int search_id_;
  QString search_text_;
  bool artist_search_;
  QList<int> requests_artist_albums_;
  QHash<int, QString> requests_album_songs_;
  QHash<int, QUrl> requests_stream_url_;
  QList<QUrl> queue_stream_url_;
  int artist_albums_requested_;
  int artist_albums_received_;
  int album_songs_requested_;
  int album_songs_received_;
  SongList songs_;
  QString search_error_;
  bool login_sent_;
  int login_attempts_;
  QUrl stream_request_url_;

};

#endif  // TIDALSERVICE_H
