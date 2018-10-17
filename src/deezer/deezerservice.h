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

#ifndef DEEZERSERVICE_H
#define DEEZERSERVICE_H

#include "config.h"

#ifdef HAVE_DZMEDIA
#  include <dzmedia.h>
#endif

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
#include "internet/internetmodel.h"
#include "internet/internetservice.h"
#include "internet/internetsearch.h"

class NetworkAccessManager;
class LocalRedirectServer;
class DeezerUrlHandler;

struct DeezerAlbumContext {
  int id;
  int search_id;
  QString artist;
  QString album;
  QString cover;
  QUrl cover_url;
};
Q_DECLARE_METATYPE(DeezerAlbumContext);

class DeezerService : public InternetService {
  Q_OBJECT

 public:
  DeezerService(Application *app, InternetModel *parent);
  ~DeezerService();

  static const Song::Source kSource;
  static const int kAppID;

  void ReloadSettings();

  void Logout();
  int Search(const QString &query, InternetSearch::SearchBy searchby);
  void CancelSearch();

  const bool app_id() { return kAppID; }
  const bool authenticated() { return !access_token_.isEmpty(); }

  void GetStreamURL(const QUrl &url);

 signals:
  void Login();
  void LoginSuccess();
  void LoginFailure(QString failure_reason);
  void Authenticated();
  void SearchResults(int id, SongList songs);
  void SearchError(int id, QString message);
  void UpdateStatus(QString text);
  void ProgressSetMaximum(int max);
  void UpdateProgress(int max);
  void StreamURLReceived(const QUrl original_url, const QUrl media_url, const Song::FileType filetype);

 public slots:
  void ShowConfig();

 private slots:
  void StartAuthorisation();
  void FetchAccessTokenFinished(QNetworkReply *reply);
  void StartSearch();
  void SearchFinished(QNetworkReply *reply, int search_id);
  void GetAlbumFinished(QNetworkReply *reply, int search_id, int album_id);
#ifdef HAVE_DZMEDIA
  void GetStreamURLFinished(const QUrl original_url, const QUrl media_url, const DZMedia::FileType dzmedia_filetype);
#endif

 private:
  void LoadAccessToken();
  void RedirectArrived(LocalRedirectServer *server, QUrl url);
  void RequestAccessToken(const QByteArray &code);
  void SetExpiryTime(int expires_in_seconds);
  void ClearSearch();
  QNetworkReply *CreateRequest(const QString &ressource_name, const QList<QPair<QString, QString>> &params);
  QByteArray GetReplyData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(QByteArray &data);
  QJsonValue ExtractData(QByteArray &data);
  void SendSearch();
  DeezerAlbumContext *CreateAlbum(const int album_id, const QString &artist, const QString &album, const QString &cover);
  void GetAlbum(const DeezerAlbumContext *album_ctx);
  Song ParseSong(const int album_id, const QString &album, const QString &album_cover, const QJsonValue &value);
  void CheckFinish();
  void Error(QString error, QVariant debug = QString());

  static const char *kApiUrl;
  static const char *kOAuthUrl;
  static const char *kOAuthAccessTokenUrl;
  static const char *kOAuthRedirectUrl;
  static const char *kSecretKey;

  NetworkAccessManager *network_;
  DeezerUrlHandler *url_handler_;
#ifdef HAVE_DZMEDIA
  DZMedia *dzmedia_;
#endif
  QTimer *timer_searchdelay_;

  QString quality_;
  int searchdelay_;
  int albumssearchlimit_;
  int songssearchlimit_;
  bool fetchalbums_;
  QString coversize_;
  bool preview_;
  QString access_token_;
  QDateTime expiry_time_;

  int pending_search_id_;
  int next_pending_search_id_;
  QString pending_search_text_;
  InternetSearch::SearchBy pending_searchby_;

  int search_id_;
  QString search_text_;
  QHash<int, DeezerAlbumContext*> requests_album_;
  QHash<int, QUrl> requests_song_;
  int albums_requested_;
  int albums_received_;
  SongList songs_;
  QString search_error_;
  QUrl stream_request_url_;

};

#endif  // DEEZERSERVICE_H
