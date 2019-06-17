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

#ifndef INTERNETSERVICE_H
#define INTERNETSERVICE_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QIcon>
#include <QSortFilterProxyModel>

#include "core/song.h"
#include "internetsearch.h"

class Application;
class CollectionBackend;
class CollectionModel;

class InternetService : public QObject {
  Q_OBJECT

 public:
  InternetService(Song::Source source, const QString &name, const QString &url_scheme, Application *app, QObject *parent = nullptr);

  virtual ~InternetService() {}

  virtual Song::Source source() const { return source_; }
  virtual QString name() const { return name_; }
  virtual QString url_scheme() const { return url_scheme_; }
  virtual bool has_initial_load_settings() const { return false; }
  virtual void InitialLoadSettings() {}
  virtual void ReloadSettings() {}
  virtual QIcon Icon() { return Song::IconForSource(source_); }
  virtual const bool oauth() { return false; }
  virtual const bool authenticated() { return false; }
  virtual int Search(const QString &query, InternetSearch::SearchType type) { return 0; }
  virtual void CancelSearch() {}

  virtual CollectionBackend *artists_collection_backend() { return nullptr; }
  virtual CollectionBackend *albums_collection_backend() { return nullptr; }
  virtual CollectionBackend *songs_collection_backend() { return nullptr; }

  virtual CollectionModel *artists_collection_model() { return nullptr; }
  virtual CollectionModel *albums_collection_model() { return nullptr; }
  virtual CollectionModel *songs_collection_model() { return nullptr; }

  virtual QSortFilterProxyModel *artists_collection_sort_model() { return nullptr; }
  virtual QSortFilterProxyModel *albums_collection_sort_model() { return nullptr; }
  virtual QSortFilterProxyModel *songs_collection_sort_model() { return nullptr; }

 public slots:
  virtual void ShowConfig() {}
  virtual void GetArtists() {}
  virtual void GetAlbums() {}
  virtual void GetSongs() {}
  virtual void ResetArtistsRequest() {}
  virtual void ResetAlbumsRequest() {}
  virtual void ResetSongsRequest() {}

 signals:
  void Login();
  void Logout();
  void Login(const QString &username, const QString &password, const QString &token);
  void Login(const QString &hostname, const int, const QString &username, const QString &password);
  void LoginSuccess();
  void LoginFailure(QString failure_reason);
  void LoginComplete(bool success, QString error = QString());

  void TestSuccess();
  void TestFailure(QString failure_reason);
  void TestComplete(bool success, QString error = QString());

  void Error(QString message);
  void Results(SongList songs);
  void UpdateStatus(QString text);
  void ProgressSetMaximum(int max);
  void UpdateProgress(int max);

  void ArtistsError(QString message);
  void ArtistsResults(SongList songs);
  void ArtistsUpdateStatus(QString text);
  void ArtistsProgressSetMaximum(int max);
  void ArtistsUpdateProgress(int max);

  void AlbumsError(QString message);
  void AlbumsResults(SongList songs);
  void AlbumsUpdateStatus(QString text);
  void AlbumsProgressSetMaximum(int max);
  void AlbumsUpdateProgress(int max);

  void SongsError(QString message);
  void SongsResults(SongList songs);
  void SongsUpdateStatus(QString text);
  void SongsProgressSetMaximum(int max);
  void SongsUpdateProgress(int max);

  void SearchResults(int id, SongList songs);
  void SearchError(int id, QString message);
  void SearchUpdateStatus(QString text);
  void SearchProgressSetMaximum(int max);
  void SearchUpdateProgress(int max);

  void AddArtists(const SongList& songs);
  void AddAlbums(const SongList& songs);
  void AddSongs(const SongList& songs);

  void RemoveArtists(const SongList& songs);
  void RemoveAlbums(const SongList& songs);
  void RemoveSongs(const SongList& songs);

  void StreamURLFinished(const QUrl original_url, const QUrl stream_url, const Song::FileType filetype, QString error = QString());

 protected:
  Application *app_;
 private:
  Song::Source source_;
  QString name_;
  QString url_scheme_;

};
Q_DECLARE_METATYPE(InternetService*);

#endif
