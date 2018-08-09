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
#include <QNetworkReply>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/song.h"
#include "internet/internetmodel.h"
#include "internet/internetservice.h"
#include "settings/tidalsettingspage.h"

class NetworkAccessManager;

struct TidalSearchContext {
  int id;
  QString text;
  QHash<int, int> requests_album_;
  QHash<int, Song *> requests_song_;
  int album_requests;
  int song_requests;
  SongList songs;
  QString error;
  bool login_sent;
  int login_attempts;
};
Q_DECLARE_METATYPE(TidalSearchContext);

class TidalService : public InternetService {
  Q_OBJECT

 public:
  TidalService(Application *app, InternetModel *parent);
  ~TidalService();

  static const char *kServiceName;

  void ReloadSettings();

  void Login(const QString &username, const QString &password);
  void Logout();
  int Search(const QString &query, TidalSettingsPage::SearchBy searchby);

  const bool login_sent() { return login_sent_; }
  const bool authenticated() { return (!session_id_.isEmpty() && !country_code_.isEmpty()); }

 signals:
  void LoginSuccess();
  void LoginFailure(QString failure_reason);
  void SearchResults(int id, SongList songs);
  void SearchError(int id, QString message);

 public slots:
  void ShowConfig();

 private slots:
  void HandleAuthReply(QNetworkReply *reply, int id);
  void StartSearch();
  void SearchFinished(QNetworkReply *reply, int id);
  void GetAlbumFinished(QNetworkReply *reply, int search_id, int album_id);
  void GetStreamURLFinished(QNetworkReply *reply, const int search_id, const int song_id);

 private:
  void Login(TidalSearchContext *search_ctx, const QString &username, const QString &password);
  void LoadSessionID();
  QNetworkReply *CreateRequest(const QString &ressource_name, const QList<QPair<QString, QString>> &params);
  QJsonObject ExtractJsonObj(TidalSearchContext *search_ctx, QNetworkReply *reply);
  QJsonArray ExtractItems(TidalSearchContext *search_ctx, QNetworkReply *reply);
  TidalSearchContext *CreateSearch(const int search_id, const QString text);
  void SendSearch(TidalSearchContext *search_ctx);
  void GetAlbum(TidalSearchContext *search_ctx, const int album_id);
  Song *ParseSong(TidalSearchContext *search_ctx, const int album_id, const QJsonValue &value);
  Song ExtractSong(TidalSearchContext *search_ctx, const QJsonValue &value);
  void GetStreamURL(TidalSearchContext *search_ctx, const int album_id, const int song_id);
  void CheckFinish(TidalSearchContext *search_ctx);
  void Error(TidalSearchContext *search_ctx, QString error, QString debug = "");

  static const char *kApiUrl;
  static const char *kAuthUrl;
  static const char *kResourcesUrl;
  static const char *kApiToken;

  NetworkAccessManager *network_;
  QTimer *search_delay_;
  int pending_search_id_;
  int next_pending_search_id_;
  int search_requests_;
  bool login_sent_;
  static const int kSearchAlbumsLimit;
  static const int kSearchTracksLimit;
  static const int kSearchDelayMsec;

  QString username_;
  QString password_;
  QString quality_;
  QString session_id_;
  quint64 user_id_;
  QString country_code_;

  QString pending_search_;
  TidalSettingsPage::SearchBy pending_searchby_;
  QHash<int, TidalSearchContext*> requests_search_;

};

#endif  // TIDALSERVICE_H
