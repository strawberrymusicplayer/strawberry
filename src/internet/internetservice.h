/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef INTERNETSERVICE_H
#define INTERNETSERVICE_H

#include <QtGlobal>
#include <QObject>
#include <QMetaType>
#include <QString>
#include <QUrl>
#include <QIcon>

#include "core/song.h"
#include "settings/settingsdialog.h"
#include "internetsearchview.h"

class Application;
class CollectionBackend;
class CollectionModel;
class CollectionFilter;

class InternetService : public QObject {
  Q_OBJECT

 public:
  explicit InternetService(Song::Source source, const QString &name, const QString &url_scheme, const QString &settings_group, SettingsDialog::Page settings_page, Application *app, QObject *parent = nullptr);

  ~InternetService() override {}
  virtual void Exit() {}

  virtual Song::Source source() const { return source_; }
  virtual QString name() const { return name_; }
  virtual QString url_scheme() const { return url_scheme_; }
  virtual QString settings_group() const { return settings_group_; }
  virtual SettingsDialog::Page settings_page() const { return settings_page_; }
  virtual bool has_initial_load_settings() const { return false; }
  virtual void InitialLoadSettings() {}
  virtual void ReloadSettings() {}
  virtual QIcon Icon() { return Song::IconForSource(source_); }
  virtual bool oauth() { return false; }
  virtual bool authenticated() { return false; }
  virtual int Search(const QString &query, InternetSearchView::SearchType type) { Q_UNUSED(query); Q_UNUSED(type); return 0; }
  virtual void CancelSearch() {}

  virtual CollectionBackend *artists_collection_backend() { return nullptr; }
  virtual CollectionBackend *albums_collection_backend() { return nullptr; }
  virtual CollectionBackend *songs_collection_backend() { return nullptr; }

  virtual CollectionModel *artists_collection_model() { return nullptr; }
  virtual CollectionModel *albums_collection_model() { return nullptr; }
  virtual CollectionModel *songs_collection_model() { return nullptr; }

  virtual CollectionFilter *artists_collection_filter_model() { return nullptr; }
  virtual CollectionFilter *albums_collection_filter_model() { return nullptr; }
  virtual CollectionFilter *songs_collection_filter_model() { return nullptr; }

 public slots:
  virtual void ShowConfig() {}
  virtual void GetArtists() {}
  virtual void GetAlbums() {}
  virtual void GetSongs() {}
  virtual void ResetArtistsRequest() {}
  virtual void ResetAlbumsRequest() {}
  virtual void ResetSongsRequest() {}

 signals:
  void ExitFinished();
  void RequestLogin();
  void RequestLogout();
  void LoginWithCredentials(QString api_token, QString username, QString password);
  void LoginWithHostname(QString hostname, int, QString username, QString password);
  void LoginSuccess();
  void LoginFailure(QString failure_reason);
  void LoginComplete(bool success, QString error = QString());

  void TestSuccess();
  void TestFailure(QString failure_reason);
  void TestComplete(bool success, QString error = QString());

  void Error(QString error);
  void Results(SongList songs, QString error);
  void UpdateStatus(QString text);
  void ProgressSetMaximum(int max);
  void UpdateProgress(int max);

  void ArtistsResults(SongList songs, QString error);
  void ArtistsUpdateStatus(QString text);
  void ArtistsProgressSetMaximum(int max);
  void ArtistsUpdateProgress(int max);

  void AlbumsResults(SongList songs, QString error);
  void AlbumsUpdateStatus(QString text);
  void AlbumsProgressSetMaximum(int max);
  void AlbumsUpdateProgress(int max);

  void SongsResults(SongList songs, QString error);
  void SongsUpdateStatus(QString text);
  void SongsProgressSetMaximum(int max);
  void SongsUpdateProgress(int max);

  void SearchResults(int id, SongList songs, QString error);
  void SearchUpdateStatus(int id, QString text);
  void SearchProgressSetMaximum(int id, int max);
  void SearchUpdateProgress(int id, int max);

  void AddArtists(SongList);
  void AddAlbums(SongList);
  void AddSongs(SongList);

  void RemoveArtists(SongList);
  void RemoveAlbums(SongList);
  void RemoveSongs(SongList);

  void StreamURLFinished(QUrl original_url, QUrl stream_url, Song::FileType filetype, int samplerate, int bit_depth, qint64 duration, QString error = QString());

 protected:
  Application *app_;

 private:
  Song::Source source_;
  QString name_;
  QString url_scheme_;
  QString settings_group_;
  SettingsDialog::Page settings_page_;

};
Q_DECLARE_METATYPE(InternetService*)

#endif  // INTERNETSERVICE_H
